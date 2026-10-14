# Omni-RISC APU

A **RISC-V Accelerated Processing Unit** — an RV32IM scalar CPU fused with a SIMT GPU on a single FPGA. Solo build, verified against the official RISC-V compliance suite, synthesized on Xilinx Artix-7 (XC7A100T).

![Omni-RISC APU architecture](docs/diagrams/architecture.png)

> Diagram generated from the RTL module hierarchy (`scripts/gen_arch_diagram.py`): solid edges are module instantiations, dashed edges are coherence relationships.

## Highlights

- **RV32IM CPU** — 5-stage in-order pipeline (IF/ID/EX/MEM/WB), full forwarding + hazard detection, M-extension (`mul`/`div`), M-mode CSRs + traps
- **Coherent 2-core research rig** — snooping MSI D-cache (write-through, write-invalidate), LR/SC spinlock, passed MP + SB litmus tests
- **SIMT GPU** — 4 warps × 4 lanes, 16-bit SIMT ISA, warp scheduler with latency hiding, scratchpad, vector/matmul/conv kernels, warp-level `BARRIER` sync
- **Firmware grown gate by gate** — a hand-written bare-metal layer on this SoC: string/memory, `printf`, a coalescing allocator, M-mode fault diagnostics, a software timer service, a UART receive path with a CRC-framed line protocol, and a runtime GPU kernel loader. Sixteen gates, all green (`./scripts/evidence.sh`)
- **Unified memory — and its measured cost** — GPU kernels read and write the CPU's own `data_bram` through a bus master and two new opcodes, with no staging window in between. It is **2.4× slower** than the MMIO window it replaces, and the reason and the three fixes are in `RESULTS.md`: a project that only reports wins is not measuring
- **SoC** — UART, timer/CLINT, GPIO, pbus MMIO decode, bare-metal firmware + scheduler
- **FreeRTOS** — V10.5.1 preemptive RTOS on the core (M-mode CLINT port): measured 1.84 µs ISR latency, 9.84 µs context switch, interactive UART shell
- **Fits on the board** — synthesis + place-and-route on XC7A100T (Vivado 2026.1, run 2026-08-20): **7,516 LUTs (11.9%), 1,580 LUT-as-memory, 2,410 FF, 64.5 BRAM tiles, 15 DSP; WNS +0.685 ns at 50 MHz → Fmax ≈ 51.8 MHz** (the synthesis-only run reported 7,669 LUTs, WNS +3.348 ns). These are the last numbers measured with a real data-dependent kernel; they **predate the runtime GPU kernel loader**, which has not been re-synthesised because there is no Vivado on the current development machine.
- **Compliance** — riscv-arch-test rv32im: **53/54** (1 skip, Zifencei `fence.i`: split I/D BRAMs)

## Interactive demo — a shell on the APU

You can type into a FreeRTOS shell running on the RV32IM core, inside the Verilator simulation, over a bit-accurate UART. `tb_soc_shell` feeds stdin at real 115200 framing (434 clk/bit — no zero-delay cheat), so the RTL deserializer sees genuine UART waveforms:

![Interactive FreeRTOS shell session](docs/shell_demo.gif)

```bash
./tests/test_shell.sh        # scripted session gate (non-interactive, CI-friendly)
./scripts/console.sh         # live terminal: raw-mode, type like minicom (Ctrl-C quits)
```

From a fresh clone, `scripts/setup_demo.sh` does the whole dance — checks the required packages, builds the shell firmware and the testbench, runs the scripted gate, then drops you into the live console:

```bash
./scripts/setup_demo.sh          # check deps -> build -> gate -> live console
./scripts/setup_demo.sh --check  # dependency check only (exit 1 if a package is missing)
./scripts/setup_demo.sh --no-run # build + gate, no interactive shell (CI-friendly)
```

> **Packages you may not have:** the demo needs `verilator` ≥ 5.0 and the RISC-V cross toolchain (`riscv64-linux-gnu-gcc` + binutils). `setup_demo.sh` names the exact package when one is missing — on Arch/CachyOS: `sudo pacman -S verilator riscv64-linux-gnu-gcc`. The firmware images (`shell.hex`, `gpu_demo.hex`) are gitignored build artifacts, so a fresh clone has to build them before `console.sh` can run.

**Shell commands** (`help` lists them all):

| Command | What it does |
|---------|--------------|
| `help` | list commands |
| `uptime` | seconds since boot (tick counter) |
| `ps` | list FreeRTOS tasks (`uxTaskGetSystemState`) |
| `ticks` | live tick-jitter histogram from the tick hook |
| `gpu` | write A/B into the GPU scratchpad, launch vector-add over MMIO, poll to completion, print checksum |
| `peek`/`poke`/`mdump` | read/write/dump memory at a hex address |
| `gpio` | read/write GPIO |
| `mtime` | read the CLINT timer |
| `heap` | report FreeRTOS heap usage |
| `reboot` | reset the SoC |
| `quit` | end the simulation (no fixed cycle cap) |

The `gpu` command exercises the CPU↔GPU coherent shared-memory window end-to-end. The session is replayable with `asciinema play docs/shell_session.cast`.

> **Terminal notes**: `console.sh` puts your terminal in raw mode (`stty raw -echo`). The shell firmware echoes each character locally, so you see what you type once. Backspace (DEL) works — the firmware erases the last character. Tab is not supported (no completion); it's silently dropped. If you see every character doubled, a second echo source is active — check your terminal emulator's local-echo setting.

## Architecture

- **CPU**: RV32IM, 5-stage in-order pipeline, full forwarding + hazard detection, M-extension, M-mode CSRs + traps
- **Coherence rig** (`dual_core_top`): two cores sharing one `data_bram` through a snooping MSI D-cache and pbus arbiter — the coherence experiments that the APU's coarse-grained GPU sync builds on
- **GPU**: SIMT engine (4 warps × 4 lanes), INT ALU, scratchpad memory, warp scheduler with latency hiding, warp-level `BARRIER`
- **CPU↔GPU shared-memory window**: the CPU writes kernel inputs into the warp's scratchpad and reads results back through `gpu_cmd_proc` host registers — no copies, coarse-grained coherence at kernel boundaries
- **FreeRTOS V10.5.1** — preemptive RTOS on the core (M-mode CLINT port): measured 1.8 µs ISR latency, 9.8 µs context switch, interactive UART shell
- **Bus**: peripheral bus (pbus) with MMIO decode — CPU ↔ UART/timer/GPIO ↔ GPU
- **Target**: Xilinx Artix-7 (XC7A100T), Vivado 2026.1 synthesis flow

Full hardware/software architecture: [`docs/architecture.md`](docs/architecture.md)

## Verification

| Area | Result |
|------|--------|
| ISA compliance | riscv-arch-test rv32im 53/54 |
| CPU pipeline | tb_cpu_top 83/83 |
| L1 caches | tb_cache 14/14, tb_icache 27/27 |
| MSI coherence | tb_dual_spin (counter → 1000, no lost updates), litmus MP + SB 200/200 each |
| GPU kernels | tb_gpu_top_kernels (vector_add, relu, matmul, conv2d, barrier). **Not re-run on this machine — it needs numpy, which is absent** — and our own notes disagree on the check count (66 vs 118), so no number is quoted here. Coverage is real; the count is unverified. |
| GPU barrier | 4-warp sync: every warp writes PRE, barriers, resumes, writes POST — no deadlock, all markers present; lone-warp no-deadlock diag |
| SoC end-to-end | tb_soc_gpu: CPU writes A/B into GPU scratchpad, launches vector-add kernel over pbus, reads C back — `C = A + B` per lane |
| FreeRTOS | tb_soc_rtos: real preemptive RTOS (V10.5.1, M-mode port) — two tasks preempt correctly, tick timing cycle-accurate |
| RTOS metrics | ISR latency 92 cyc avg (1.84 µs), 2-switch yield round trip 492 cyc (9.84 µs), tick jitter ±194 cyc over 1200+ ticks |
| RTOS + GPU | tb_soc_rtos_q: 1000 items through a depth-8 `xQueue` in strict order; shell `gpu` command + ISR-semaphore path |
| Interactive shell | tb_soc_shell + tests/test_shell.sh: typed FreeRTOS shell over honest-baud UART, GPU kernel launched from a shell command |
| Bare-metal firmware track | F1–F8 gates, all green: string/memory 130/130, conversions 260/260, printf 93/93 + two streams byte-identical to glibc, UART RX burst 4/4 and line+timeout 5/5, CRC framing 10/10 against a receiver simulator, allocator 12/12 with a 2000-operation soak, fault handler 4/4 kinds, timers 13/13, runtime kernel loader 5/5 in both the pushed and not-pushed cases |
| Characterization | `tests/test_bench.sh` — cycles per unit of work for 7 CPU workloads read from the core's own `mcycle`, cross-checked against the CLINT's independent `mtime` (they agree to 0.03%) |
| GPU workload | `tests/test_mlp.sh` — a 2-layer MLP over 4 samples in parallel, with the GPU's kernel image reloaded four times mid-computation, and the GPU answer checked per lane against a scalar reference |
| GPU unified memory | `tests/test_gpu_mem.sh` — a kernel adds and stores back into the CPU's own C array, both directions verified; `tests/test_mlp_mem.sh` — the same MLP with no staging window at all |
| Pipeline cost | `tests/test_cpi.sh` — 1.12 CPI straight-line, 2.00 on a taken-branch loop; `tests/test_launch_sweep.sh` — separates a launch's fixed cost from its per-instruction cost |
| Continuous integration | `.github/workflows/gates.yml` — the RTL testbenches and all sixteen firmware gates on every push |

## What was measured, and what was not

**[`RESULTS.md`](RESULTS.md) carries every claim with the command that reproduces it and what
would falsify it, and `./scripts/evidence.sh` regenerates the lot in one run** — that
regenerability is the substitute for a board. What follows is the short version.

Everything above is **simulation**. Verilator models this RTL cycle for cycle, and the
area/timing figures come from Vivado 2026.1 reports. **No bitstream has been built for a
board and nothing has run on silicon** — so there is no measured Fmax, no power figure, and
no silicon-versus-simulation comparison to offer. Claiming otherwise would be the easy lie.

The cycle numbers are from the model at a nominal 50 MHz. They are trustworthy *relative to
each other* — two independent counters agree, and every workload's cost grows with its input —
and they are not a statement about wall-clock speed.

A few of them are worth reading:

| Measurement | Value |
|---|---|
| `memcpy`, byte-wise (this project's own library) | 24.1 cycles/byte |
| `memset` / FNV-1a hash | 21.1 / 12.0 cycles/byte |
| scalar 2×2 matmul | 15.1–19.1 cycles per multiply-accumulate |
| insertion sort | 41 → 173 cycles/element as n goes 32 → 64 (quadratic, as expected) |
| GPU launch → completion interrupt | **306 cycles** (against 7 cycles for the same 16 additions done scalar — 43.7×) |
| a 4-sample 2-layer MLP, kernel reloaded per layer | **6,278 GPU cycles vs 233 scalar cycles** |
| the same MLP over shared memory (unified, no staging) | **15,125 cycles — 2.4× worse**, ~40 cycles per global access |
| **CPI**, straight-line / taken-branch loop | **1.12 / 2.00** — the cost of having no branch predictor |
| **what a launch costs**, fixed and marginal | **190 cycles + ~4 per kernel instruction** (n=4 → 197, n=64 → 450) |

The MLP rows are the honest headline rather than a flattering one: at this size the GPU is
**26.9× more expensive** than the CPU doing the same arithmetic, because each launch costs
~306 cycles of MMIO, trap and interrupt latency to buy ~29 integer multiply-accumulates. The
dispatch path is the design's real constraint, and the number above is how you would measure
whether a bigger kernel or batched launches ever pay it off.

The row after it is the reason this project does not claim "unified memory made it faster": it
did not. The GPU can now read and write the CPU's own arrays directly (no MMIO window, no
staging copies — see `tests/test_gpu_mem.sh`, which passes), and the MLP ported onto that path
is correct but **2.4× slower** than the window it replaced, because each of its ~224 global
accesses is a bus transaction that arbitrates against a CPU polling memory in its wait loop,
where a scratchpad access costs one cycle. Cost moved from the CPU's staging loop into the
GPU's access path. The fixes — poll only a peripheral while waiting, give the GPU priority
during a kernel, allow per-lane addresses — are identified and unbuilt.

**CPI, measured where the instruction count is exact.** Straight-line code runs at **1.12
CPI** (72 cycles / 64 instructions) and a taken-branch loop at **2.00** — which is what the
absence of a branch predictor costs, measured rather than asserted (`tests/test_cpi.sh`). The
CSR route stays unusable: `minstret` is readable but the pipeline has no instruction-retired
signal to count it from, and plumbing one correctly means a valid bit through all four
pipeline registers — which is exactly the hold/flush timing that has already produced two
bugs here — so CPI comes from loops whose length is known by construction instead. (`mcycle`
was not counting at all until the characterization work found it declared, readable,
writable and silent.)

**One open bug, recorded rather than hidden.** A launch of a pure-ALU kernel intermittently
never signals completion. Those kernels contain no `LDG`/`STG`, so it is the GPU completion
path, not the new memory path; `tests/test_launch_sweep.sh` prints the same kernel twice so
the flakiness is visible in its own output instead of being averaged away.

## How the testbenches work

Everything runs through **Verilator**: each testbench is a small **C++ harness** that drives the RTL module directly through its ports, checks results, and exits `0` on pass.

```
hardware/sim/<path>/tb_<name>.cpp   — the C++ harness
hardware/rtl/**/<name>.v            — the DUT (module name == file name)
```

- `./hardware/sim/run_sim.sh <path>/tb_<name>` compiles the harness + DUT with Verilator, runs it, and reports **PASS/FAIL from the exit code** (`0` = pass).
- The DUT is auto-derived from the TB name — but some TB names map to a different top module (baked into `run_sim.sh`):

  | TB name | DUT |
  |---------|-----|
  | `tb_soc_*` | `soc_top` |
  | `tb_cache` | `l1_dcache` |
  | `tb_icache` | `l1_icache` |
  | `tb_compliance` | `cpu_top` |
  | `tb_dual_*` / `tb_litmus_*` | `dual_core_top` |
  | `tb_gpu_top_*` | `gpu_top` |
  | `tb_uart_rx` | `uart` |

- Verilator flags: `--cc --exe --trace --Wall -Wno-UNUSED -Wno-UNDRIVEN`; build output lands in `hardware/sim/obj_dir_<tb>`, waveforms in `hardware/sim/waves/<tb>.vcd`.
- **The RTL conforms to the TB, not the other way round** — port names and instruction encodings are fixed by the existing TBs (e.g. the ALU opcode map is defined by `tb_alu.cpp`). When adding a module, match the harness it must pass.
- Some TBs auto-stage firmware/GPU hex (`tb_soc_gpu`, `tb_soc_shell`, `tb_dual_spin`); after `make clean` (which deletes all `*.hex`) re-run the affected TB so the staged images are regenerated.

### Running things

```bash
# One testbench (make wrapper)
make sim TB=cpu/tb_alu

# Direct, with waveform + GTKWave
./hardware/sim/run_sim.sh cpu/tb_alu --wave

# The interactive shell (type into the simulated APU)
./scripts/console.sh
./tests/test_shell.sh          # scripted, non-interactive gate

# The full GPU kernel battery (vector_add, relu, matmul, conv2d, barrier)
./hardware/sim/run_sim.sh gpu/tb_gpu_top_kernels
```

### Adding a new testbench

1. Write `hardware/sim/<path>/tb_<name>.cpp` with the module under test instantiated via Verilator's generated header (`V<dut>.h`).
2. Keep the DUT file at `hardware/rtl/**/<name>.v` with `module name(...)` matching the file name.
3. `./hardware/sim/run_sim.sh <path>/tb_<name>` — it auto-derives the DUT, builds, runs, reports PASS/FAIL.
4. Exit `0` = pass, non-zero = fail; `run_sim.sh` turns that into the final verdict.

## GPU ISA and kernels

The GPU is a SIMT engine: **4 warps × 4 lanes**, each warp with its own 8-register × 4-lane file and a 4-bank scratchpad (lane *l* → bank *l*). Instructions are 16-bit:

```
[15:12]op [11:9]rd [8:6]rs1 [5:3]rs2 [2:0]f3

op 0  ALU     f3: ADD SUB AND OR XOR SLT SLTU SLL
op 1  LSU     bit0: 0 = LD rd <- sp[rs1], 1 = ST sp[rs1] <- rs2
op 2  BR      instr[7:0] = byte target (warp-level, no lane predication)
op 3  MUL     legacy alias for multiply
op 4  ALU2    f3: SRL SRA MUL
op 5  LDI     rd <- signext(instr[8:0])
op 6  BARRIER warp-level sync: wait for all active warps to arrive
op F  HALT
```

Kernels are written in `firmware/gpu_kernels/*.S` and assembled with `scripts/gpu_asm.py`:

```bash
python3 scripts/gpu_asm.py firmware/gpu_kernels/my_kernel.S
# -> firmware/gpu_kernels/my_kernel.hex   ($readmemh image)
```

The `.hex` is **flashed into GPU IMEM at synthesis time** (the `IMEM_FILE` parameter, `gpu_demo.hex`), **pushed in at run time** through the load port (`GPU_IMEM_ADDR`/`GPU_IMEM_DATA` — see F8 in `docs/plan_firmware_track.md`, so a kernel image can change mid-computation), or **loaded directly by a testbench** (e.g. `tb_gpu_top_kernels` calls `HEX("vector_add", 0x000)`). Reference vectors for the RTL TBs are generated by `tests/test_gpu_kernels.py`.

The `BARRIER` opcode parks a warp in a WAIT state until every active warp has issued its own `BARRIER`, then releases them together (a lone warp passes immediately; a warp that never reaches the barrier deadlocks the group — undefined behavior, same as CUDA). See [`docs/gpu_design.md`](docs/gpu_design.md) for the microarchitecture.

## Project Structure

```
hardware/rtl/cpu/     — RISC-V CPU core (5-stage pipeline, caches, CSRs, 2-core MSI)
hardware/rtl/gpu/     — SIMT GPU (warp scheduler, exec lanes, scratchpad, BARRIER)
hardware/rtl/memory/  — instruction/data BRAM
hardware/rtl/soc/     — UART, CLINT timer, GPIO, SoC integration (soc_top, dual_core_top)
hardware/rtl/bus/     — pbus_arbiter (2-core peripheral arbitration), mem_arbiter (CPU + GPU against the shared data memory)
hardware/sim/         — Verilator testbenches + run_sim.sh driver
firmware/             — bare-metal RISC-V firmware (boot, drivers, apps, GPU kernels)
scripts/              — elf_to_hex.py, gpu_asm.py (16-bit SIMT assembler)
tests/                — riscv-arch-test compliance runner, GPU kernel tests, shell gate
docs/                 — architecture, gpu_design, memory_map, compliance results
```

## Quick Start

```bash
# Build + run a single testbench (Verilator)
make sim TB=cpu/tb_alu

# Cross-compile firmware
make firmware              # default APP=hello; apps live in firmware/apps/
make firmware APP=rtos     # FreeRTOS apps (rtos, rtos_q, rtos_sem, shell)

# RISC-V compliance suite (53/54)
make compliance

# Vivado synthesis + implementation (place+route)
make synth                 # needs Vivado on PATH or make VIVADO=...
make impl
```

## Requirements

| Package | Needed for | Arch/CachyOS |
|---------|------------|--------------|
| Verilator ≥ 5.0 | every testbench (`make sim`, `run_sim.sh`) | `verilator` |
| `riscv64-linux-gnu-gcc` (rv32 multilib, + binutils) | firmware and spinlock images | `riscv64-linux-gnu-gcc` |
| Python 3 | `elf_to_hex.py`, `gpu_asm.py`, `tests/test_gpu_kernels.py` | `python` |
| `numpy`, `pyelftools` | `tests/test_gpu_kernels.py`, `scripts/elf_to_hex.py` | `python-numpy`, `python-pyelftools` |
| GTKWave | `run_sim.sh --wave` only | `gtkwave` |
| asciinema | replaying `docs/shell_session.cast` only | `asciinema` |
| Pillow | regenerating `docs/shell_demo.gif` only | `python-pillow` |
| Vivado 2026.1 | `make synth` / `make impl` only | `/tools/2026.1/Vivado/bin/vivado`, override with `make VIVADO=...` |

`./scripts/setup_demo.sh --check` verifies the demo set for you. Override the toolchain prefix with `make PREFIX=...` or `TOOLCHAIN_PREFIX=... ./scripts/setup_demo.sh`.

## Where to look for details

| Topic | Doc |
|-------|-----|
| System architecture + coherence story | [`docs/architecture.md`](docs/architecture.md) |
| CPU pipeline spec + bring-up order | [`docs/cpu_design.md`](docs/cpu_design.md) |
| GPU microarchitecture + ISA | [`docs/gpu_design.md`](docs/gpu_design.md) |
| SoC address map (pbus decode) | [`docs/memory_map.md`](docs/memory_map.md) |
| riscv-arch-test results | [`docs/compliance_results.md`](docs/compliance_results.md) |
| FreeRTOS port notes + bug postmortems | [`docs/rtos_bringup.md`](docs/rtos_bringup.md) |
| Locked plan, priority tiers | [`docs/history/roadmap.md`](docs/history/roadmap.md) |

## License

Apache-2.0 — see [LICENSE](LICENSE).

## References

- RISC-V ISA Manual
- riscv-arch-test (official compliance suite)