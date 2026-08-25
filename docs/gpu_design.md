# Omni-RISC APU — GPU Design

The SIMT engine as built: a 4-warp × 4-lane vector machine with its own
frontend, attached to the pbus as a memory-mapped command slave. Companion
docs: `architecture.md` (system view), `memory_map.md` (register addresses).

## Instruction set (16-bit fixed width)

```
[15:12]op [11:9]rd [8:6]rs1 [5:3]rs2 [2:0]f3

op 0  ALU   f3: ADD SUB AND OR XOR SLT SLTU SLL
op 4  ALU2  f3: SRL SRA MUL
op 1  LSU   bit0: 0 = LD rd <- sp[rs1], 1 = ST sp[rs1] <- rs2
op 2  BR    instr[7:0] = byte target
op 5  LDI   rd <- signext(instr[8:0])
op F  HALT
```

- 8 registers per lane (`r0`–`r7`), 32-bit datapath.
- Assembled by `scripts/gpu_asm.py` into a `$readmemh` hex image; reference
  vectors come from `tests/test_gpu_kernels.py`. Kernels live in
  `firmware/gpu_kernels/*.S` and are **flashed into GPU IMEM at synthesis
  time** (`IMEM_FILE` parameter, `gpu_demo.hex`) — there is no runtime code
  download; the CPU can only launch what was compiled in.
- No branches on individual lanes: `BR` is warp-level (all lanes together),
  based on scalar flag state; there is no per-lane predication or mask
  register. Divergence within a warp is not representable.

## Microarchitecture (`hardware/rtl/gpu/`)

```
        pbus slave regs (gpu_cmd_proc)
                 |
   +-------------v--------------+
   | warp_scheduler (4 warps)   |  per-warp PC + status (READY/RUNNING/DONE)
   +------+---------------------+
          | round-robin over ready warps (latency hiding)
   +------v---------------------+
   | gpu_fetch -> gpu_decode    |  shared frontend, one warp per cycle
   +------+---------------------+
          | instruction + active warp
   +------v---------------------+
   | exec_lane x4 (gpu_alu,     |  all 4 lanes execute the same instruction
   | gpu_lsu)                   |  on their own register operands
   +------v---------v----------+
   | gpu_regfile   gpu_scratchpad (4 banks x 32 words x 32-bit)
```

- **warp_scheduler**: per-warp PC and 2-bit status; a warp in READY is
  issued round-robin, HALT moves it to DONE. Multiple READY warps hide
  scratchpad/ALU latency — the SIMT analog of a scoreboard.
- **Registers**: `gpu_regfile` — per-warp, per-lane register file
  (4 warps × 4 lanes × 8 regs × 32-bit). Implemented as LUTRAM with **no
  reset** (BRAM/LUTRAM cannot be reset; adding one flattens it to FFs).
- **Scratchpad**: 4 banks (one per lane) × 32 words × 32-bit, indexed by
  the per-lane address. Each bank has **exactly one write port**, shared
  between the lane's own stores and host-window writes (host has priority —
  safe because the coherence protocol guarantees write-before-read). A
  second write port would flatten it to muxes/FFs (~54k LUT explosion, the
  known failure mode).

## Host interface (`gpu_cmd_proc.v`, base `0x4000_2000`)

| Offset  | Register   | Access | Effect                                                    |
|---------|------------|--------|-----------------------------------------------------------|
| `+0x00` | `WARP_PC0` | W      | launch PC for warp 0 (word address)                       |
| `+0x10` | `LAUNCH`   | W      | bit31 = go, `[1:0]` = warp id                             |
| `+0x14` | `RESULT`   | R      | warp0 scratchpad word 0 (readback shortcut)               |
| `+0x18` | `HOST_WIN` | W      | `{bank[1:0], word[7:0]}` — selects a scratchpad word      |
| `+0x1C` | `HOST_DATA`| R/W    | the selected word, lane `bank` (32-bit)                   |
| `+0x20` | `STATUS`   | R      | low nibble = active-warps bitmap; poll until 0            |

## Kernel launch protocol (the coherence contract)

The CPU↔GPU shared-memory window is the whole coherence story — coarse-
grained, at kernel boundaries, no snooping:

1. **Write inputs (release)**: for each lane `i`, `HOST_WIN = {i, word}`,
   then store `A[i]`/`B[i]` through `HOST_DATA`. Host writes have port
   priority over lane stores; the GPU is not running, so there is no race.
2. **Launch (acquire)**: `WARP_PC0 = kernel entry`, `LAUNCH = 0x80000000`.
3. **Poll**: read `STATUS` until the warp's bit clears (kernel HALTed).
4. **Read results**: same `HOST_WIN`/`HOST_DATA` window; `C[i]` lands in
   scratchpad word 32 in the vector-add convention.

`firmware/apps/benchmark_gpu.c` and the shell's `gpu` command both follow
exactly this sequence; `tb_soc_gpu` verifies `C = A + B` per lane end-to-end
through the real pipeline.

## Synthesis constraints (hard-won, do not regress)

- `gpu_regfile` and `gpu_scratchpad` arrays must have **no reset** and the
  scratchpad must keep **one write port per bank** — both are what keep
  them RAM-inferable. Re-introducing either flattens the GPU to ~54k LUTs
  (35,841 regfile + 17,474 scratchpad + 32,768 FF in the measured failure).
- Kernel hex must be a real, data-dependent kernel at `make synth` time:
  an empty/stub image constant-folds the entire datapath and produces
  meaningless utilization numbers.
