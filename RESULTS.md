# Results — every claim, how it was measured, and how to break it

This file exists because there is no board. With no silicon, the only thing that
makes a claim about this project worth reading is that **someone else can re-run
it**, so every row below names the command that produced it and what would falsify
it. Nothing here is hand-typed from a log.

```bash
./scripts/evidence.sh        # regenerates every number in this file
```

**Scope of evidence: simulation.** Verilator models this RTL cycle for cycle at a
nominal 50 MHz. No bitstream exists, nothing has run on silicon, and there is no
measured Fmax or power figure. The one area/timing row is from a Vivado run on
2026-08-20 and is labelled as such — **everything built after that date is
unsynthesised**, including the runtime GPU kernel loader and unified memory.

## Functional verification

| Claim | Measured how | Reproduce | Falsified by |
|---|---|---|---|
| RV32IM ISA compliance 53/54 (1 skip) | riscv-arch-test rv32im, signatures diffed against `tests/omni-risc/ref_sim.cpp` | `make compliance` | any signature mismatch; the skip is `fence.i` (split I/D BRAMs make self-modifying code impossible) |
| CPU pipeline correct | 83 self-checking cases in `tb_cpu_top` | `./hardware/sim/run_sim.sh cpu/tb_cpu_top` | any FAIL line |
| ALU conforms to its encoder contract | 42 cases in `tb_alu` | `./hardware/sim/run_sim.sh cpu/tb_alu` | any FAIL line |
| 2-core MSI coherence, no lost updates | spinlock counter → 1000; litmus MP + SB 200/200 | `make sim TB=dual_core/tb_dual_spin` | a counter value ≠ 1000, or any litmus run failing |
| FreeRTOS schedules and preempts | `tb_soc_rtos`: task A ticks 1–4 at 25.05 M cycles, task B at 60.09 M | `./hardware/sim/run_sim.sh soc/tb_soc_rtos` | tick spacing drifting from the expected 500 ms/1200 ms @ 50 MHz |
| ISR latency 92 cycles avg, 2-switch yield 492 cycles | `tb_soc_rtos_metrics`, 1200+ ticks | `./hardware/sim/run_sim.sh soc/tb_soc_rtos_metrics` | latencies outside the printed distribution |
| GPU runs real kernels over MMIO | `tb_soc_gpu`: CPU writes A/B to the warp0 window, kernel computes, readback `C = A + B` | `./hardware/sim/run_sim.sh soc/tb_soc_gpu` | any lane whose readback ≠ A + B |
| A kernel can be pushed at run time, not flashed | `test_gpu_loader`: 30 words pushed over the load port, `imem_addr=30`, launched, completion IRQ; and with **nothing** pushed there is no completion and no result | `./tests/test_gpu_loader.sh` | the not-pushed case ever producing a result |
| The GPU reads and writes the CPU's own memory | `test_gpu_mem`: kernel adds the CPU's array words and stores back, `c=333`, store lands at the expected index | `./tests/test_gpu_mem.sh` | `found333` ≠ the expected index |

## Firmware track (F1–F8)

| Claim | Reproduce | Falsified by |
|---|---|---|
| string/memory library, 130 checks | `./tests/test_libc.sh` | any vector failing |
| numeric conversions (saturating, per-call `OMNI_ERANGE`), 260 checks | `./tests/test_convert.sh` | a case where the host glibc disagrees |
| printf, 93 vectors **plus** two streams byte-identical to glibc | `./tests/test_printf.sh` | any byte difference from the host C library's output |
| UART RX accounts for loss (16-byte FIFO deliberately overrun), burst 4/4 + paced line/timeout 5/5 | `./tests/test_uart.sh` | consumed + reconstructed loss ≠ sent |
| CRC-framed host protocol, 10 checks, reference is a byte-level receiver simulator | `./tests/test_framing.sh` | a frame the simulator and the firmware decode differently |
| allocator, 12 checks, 2000-op randomised soak with `heap_check()` after **every** operation, `leaks=0` | `./tests/test_alloc.sh` | the soak naming a failing op; or the arena not returning to one free block |
| fault diagnostics, 4 kinds (illegal, misaligned, ecall, ebreak) | `./tests/test_fault.sh` | a kind that traps without a dump, or a dump with the wrong `mcause` |
| time and software timers, 13 checks | `./tests/test_timers.sh` | a one-shot firing twice, or a cancelled timer firing |
| the shell demo end to end, 17 checks | `./tests/test_shell.sh` | any assertion in `tests/shell_session.txt` |

## Characterization (simulated cycles)

| Workload | Measured | Reproduce | Falsified by |
|---|---|---|---|
| `memcpy`, byte-wise | 24.1 cycles/byte | `./tests/test_bench.sh` | per-unit cost changing with input size (checked: stays within 1%) |
| `memset` / FNV-1a | 21.1 / 12.0 cycles/byte | same | same |
| insertion sort | 41 → 173 cycles/element (n=32 → 64) | same | the ratio leaving the quadratic range |
| scalar 2×2 matmul | 15.1–19.1 cycles per multiply-accumulate | same | per-MAC cost not falling as the loop amortises |
| `snprintf` | ~3,894 cycles/line | same | — |
| malloc+free pair | 434 → 696 cycles, 16 → 32 pairs | same | the count not rising under arena pressure |
| **GPU launch → completion interrupt** | **306 cycles**, against 7 cycles for the same 16 additions scalar (43.7×) | same | the IRQ arriving before the warp halts, or the two counters disagreeing |
| two independent counters agree | core `mcycle` vs CLINT `mtime`, within 0.03% on every window | same | skew > 5% (the app asserts this) |
| **CPI, straight-line code** | **1.12** (72 cycles / 64 instructions, no branches) | `./tests/test_cpi.sh` | CPI outside 0.9–2.0; the gate re-derives it from the printed cycles and counts rather than trusting the app |
| **CPI, taken-branch loop** | **2.00** (16,384 cycles / 8,192 instructions) | same | a taken branch not costing more per instruction than straight-line code — this is what the absence of a branch predictor costs, measured |

## Where the design's cost actually is

| Workload | Result | Reproduce | Why it matters |
|---|---|---|---|
| 4-sample 2-layer MLP, kernel image **reloaded** per layer (MMIO staging window) | **6,278 GPU cycles vs 233 scalar** — the GPU is 26.9× more expensive | `./tests/test_mlp.sh` | a launch costs ~306 cycles to buy ~29 integer MACs; dispatch dominates at this size |
| the same MLP on **unified memory** (no staging window, kernels use LDG/STG) | **15,125 cycles — 2.4× worse**, ~40 cycles per global access | `./tests/test_mlp_mem.sh` | unification moved cost from the CPU's staging loop into the GPU's access path; each access arbitrates against a CPU polling memory in its wait loop |
| **what a launch costs, split** | **190 cycles fixed + ~4 cycles per kernel instruction** (n=4: 197, n=64: 450) | `./tests/test_launch_sweep.sh` | the fitted intercept leaving 30–4,000, or a longer kernel not costing more |

The last two rows together are the design's central tension, quantified: a launch
costs ~190 cycles before it does any work, so a kernel must carry **more than ~190
cycles' worth of useful work** before dispatching it beats doing that work on the
CPU. No kernel in this project reaches that bar — the MLP's four launches carry
about 29 integer multiply-accumulates — which is precisely why it loses to the
scalar path by 26.9×, and it says what would have to change (much bigger kernels,
or batched launches) for the GPU to pay for itself here.

**Open bug, found while measuring this and recorded rather than hidden:** launching
a pure-ALU kernel intermittently never signals completion. The sweep kernels contain
no `LDG`/`STG`, so the unified-memory handshake is not involved — it is in the
GPU/warp-scheduler completion path. The sweep prints the same n=16 kernel twice
(`[SWEEP] flaky ...`) so the flakiness is visible; it did not reproduce in the run
that produced the numbers above, which is itself the point.

These two rows are the honest headline and they are not flattering. They are also
the most useful thing in this file: they say where the architecture's bottleneck
is, quantitatively, and name the three fixes (poll only a peripheral while waiting,
give the GPU priority during a kernel, allow per-lane addresses). A project that
only reports wins is not measuring.

## Area and timing

| Claim | Source | Status |
|---|---|---|
| 7,516 LUTs (11.9%), 1,580 LUT-as-memory, 2,410 FF, 64.5 BRAM, 15 DSP; WNS +0.685 ns @ 50 MHz → ~51.8 MHz, xc7a100t | **Vivado 2026.1 run of 2026-08-20** (synth-only run: 7,669 LUTs, WNS +3.348 ns) | **not re-measurable here** — no Vivado on this machine. Predates the GPU load port and unified memory, which are therefore unsynthesised. |

## Not measured at all

- **Anything on silicon.** No bitstream, no board, no measured clock, no power, no
  silicon-vs-simulation gap.
- **CPI.** `minstret` is readable but the core has no instruction-retired signal to
  count it from, so it stays 0; any CPI published today would be a guess. (`mcycle`
  was not counting either until the characterization work found it silent.)
- **Post-August area/timing**, per the row above.

## Reproducing the whole thing from a fresh clone

```bash
git clone <repo> && cd Omni-RISC
./scripts/setup_demo.sh --check     # what is missing, and the package that provides it
./scripts/setup_demo.sh --no-run    # build firmware + testbench, run the shell gate
./scripts/evidence.sh               # every number above
```

Requirements are in the README. Everything the demo needs besides packages is in
git; all `*.hex` are build artifacts and are regenerated.
