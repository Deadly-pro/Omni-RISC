# Omni-RISC: a coherent CPU+GPU APU, built and verified without silicon

> **Draft.** This is the technical content in paper shape so it can be turned into a
> submission or read directly by an interviewer. The numbers are real and traceable
> (`RESULTS.md`, `./scripts/evidence.sh`); the framing, venue and voice are yours to
> set. `[TODO]` marks the places only you can fill.

## Abstract

This report describes Omni-RISC, a RISC-V Accelerated Processing Unit built by one
person: an RV32IM in-order core, a SIMT GPU, two scalar cores with fine-grained
snooping MSI coherence, an L1 cache pair, a SoC with UART/timer/GPIO, a FreeRTOS
port, a hand-written bare-metal library layer, and a runtime GPU kernel loader.
It covers the design, but its subject is the *verification*: how much can be
established about a processor when no board is available, and what that costs.

Everything here is simulation — Verilator for cycles, Yosys/Vivado reports for area.
No bitstream has been built and nothing has run on silicon. The contribution is
therefore not a performance claim but a methodology: a claims table where every
number names the command that reproduces it and the observation that would falsify
it, reference models written instead of expected values chosen, and results recorded
whether or not they flatter the design.

## 1. Why

`[TODO: your reason - the course, the portfolio, the thing you wanted to understand]`

Three constraints shaped the project: solo work, no FPGA board, and a deliberate
scope guard. The guard is worth stating because it is enforced rather than
aspirational: no out-of-order execution, no branch predictor, no MESI, no
multi-level caches, no MMU, no Linux. Each is a well-known direction to grow, and
each was refused in writing so that a complete, verified design would exist rather
than an impressive fragment. A solo project's failure mode is breadth.

## 2. Design

**CPU.** RV32IM, five stages (IF/ID/EX/MEM/WB), full forwarding and hazard
detection, M-mode CSRs and traps, single-cycle multiply and an iterative divider.
Instruction and data memories are separate 256 KB BRAMs, which is why
self-modifying `fence.i` is unsupported and skipped in compliance (53/54).

**Coherence.** Two scalar cores share a data memory through a snooping MSI
protocol — write-through, write-invalidate, two agents only — with a shared
data BRAM behind a snoop bus. The GPU participates coarsely: the CPU owns its
data, and kernel boundaries are where acquire/release happen.

**GPU.** A 16-bit SIMT ISA, four warps of four lanes, a round-robin scheduler with
latency hiding, per-lane register files, four-bank scratchpads, warp-level
`BARRIER`. Kernels are assembled from vendored `.S` sources by a small assembler.

**SoC and firmware.** UART (full duplex, 16-byte RX FIFO with overrun accounting),
CLINT, GPIO, a peripheral bus, and a pbus-decoded GPU command interface. FreeRTOS
V10.5.1 runs on it through an M-mode CLINT port. Above that, a hand-written
freestanding layer: string/memory, `printf`, a coalescing allocator, fault
diagnostics, a software timer service, and a UART receive path with a line
protocol.

**Two things this report adds to that design**, both on the GPU's data path:
a *runtime kernel loader* (kernels are pushed into GPU instruction memory over the
bus instead of being flashed at synthesis time, so a kernel image can change
mid-computation) and *unified memory* (the GPU reaches the same `data_bram` the
CPU's ordinary loads and stores use, via a bus master and two new opcodes, so no
staging window is needed). See §4 for what the second one cost.

## 3. Verification methodology

This is the part with generalisable content.

**Reference models instead of chosen expectations.** Where a "correct" answer exists
outside this project, it is computed by something independent rather than written
down by the author:

| Subject | Reference |
|---|---|
| ISA compliance | a from-scratch RV32IM(+Zicsr) interpreter (`tests/omni-risc/ref_sim.cpp`), diffed per test |
| `printf` | the host C library's `snprintf`, plus byte-identical stream comparison |
| `strtoul`/`strtol` | the host C library, cross-checked against the gate's own Python model at build time |
| CRC-framed receive path | a byte-level simulator of the receiver state machine |
| GPU kernels | an independent Python model of each kernel's arithmetic |

The point is that a test whose expected values were written by the same person who
wrote the implementation mostly tests agreement, not correctness.

**Gates, not demo programs.** Each capability has a gate: a firmware app run on the
simulated SoC through a generic console testbench, asserting its own families and
printing machine-checkable lines. Sixteen of them exist; `./scripts/evidence.sh`
runs the lot.

**Instrument to measure, not to guess.** Cycling counters exposed a design gap
(`mcycle`/`minstret` were readable but never incremented — the core advertised
performance counters that did not count). Two bugs in the new GPU data path were
found by adding three read-only debug registers rather than by reasoning about the
RTL; one of them, a completion flag consumed by the wrong warp, only showed up as an
intermittent timeout and would have passed any short test.

**Claims are falsifiable and labelled.** `RESULTS.md` pairs each claim with the
command that reproduces it *and* what would falsify it, and separates what was
measured from what was not measured at all.

## 4. Measurements

All cycles are from the Verilator model at a nominal 50 MHz. Reproduce with
`./scripts/evidence.sh`; per-item commands are in `RESULTS.md`.

**Pipeline.** Straight-line code: **1.12 CPI** (72 cycles / 64 instructions).
A taken-branch loop: **2.00 CPI** (16,384 / 8,192). The difference is what the
absence of a branch predictor costs, measured rather than asserted. CPI comes from
instruction counts known by construction — the CSRs cannot supply it, because the
pipeline has no instruction-retired signal.

**Workload cost.**

| Workload | Cycles |
|---|---|
| `memcpy`, byte-wise | 24.1 / byte |
| `memset` / FNV-1a | 21.1 / 12.0 / byte |
| scalar 2×2 matmul | 15.1–19.1 / multiply-accumulate |
| insertion sort | 41 → 173 / element (n=32 → 64) |
| `snprintf` | ~3,894 / line |
| GPU launch → completion interrupt | 306 |

**Where the design's cost is.** A launch costs **190 cycles fixed plus ~4 cycles per
kernel instruction**. A four-sample, two-layer MLP with the kernel image reloaded
per layer costs **6,278 GPU cycles against 233 scalar** — the GPU is 26.9× *more*
expensive, because four launches at ~190 cycles each carry about 29 integer
multiply-accumulates. Porting that workload onto unified memory removed the staging
cost entirely and made it **worse** — 15,125 cycles, ~40 cycles per global access —
because each access arbitrates against a CPU polling memory in its wait loop.
Unification moved cost from the CPU's staging loop into the GPU's access path.

**The honest summary of §4**: this design's GPU cannot pay for itself at the kernel
sizes it has been given, and the measurements say by how much and why. That is a
more useful result than a favourable one, and it names its own fix: either kernels
large enough to amortise ~190 cycles of dispatch, or batched launches.

**Area and timing.** 7,516 LUTs (11.9%), 2,410 FF, 64.5 BRAM tiles, WNS +0.685 ns →
~51.8 MHz on XC7A100T, from a Vivado 2026.1 run of 2026-08-20. That predates the
runtime kernel loader and unified memory, which are unsynthesised — there is no
Vivado on the machine this work continued on.

## 5. Threats to validity

Stated plainly, because they are the first thing a careful reader will ask:

1. **No silicon.** No bitstream, no measured clock, no power, no
   silicon-versus-simulation comparison. Cycle numbers are trustworthy *relative* to
   each other and are not a wall-clock claim.
2. **No timing for recent work.** Area/timing is an August measurement of an earlier
   revision.
3. **Simulation is a model.** A Verilator bug or a testbench misread would survive
   every gate. Partially mitigated by using two independent counters (`mcycle` vs
   the CLINT's `mtime`, agreeing to 0.03%) and by a testbench whose UART framing is
   bit-accurate rather than zero-delay.
4. **One intermittent bug is open**: a pure-ALU kernel launch occasionally never
   signals completion. It is in the GPU completion path, not the new memory path,
   and it is recorded in `RESULTS.md` rather than papered over.
5. **Single author, no external users.** Every claim here is self-reported and
   self-verified.

## 6. What I would do next

- Fix the intermittent completion (a gate that passes by luck is not evidence).
- Evaluate the three identified fixes for the unified-memory regression: poll only a
  peripheral while waiting, give the GPU priority during a kernel, and allow
  per-lane global addresses so four SIMT lanes work in parallel again.
- Bring the recent work under synthesis (Yosys at minimum; Vivado when available) so
  the area cost of the loader, the arbiter and the GPU master is known.
- Formalise more of the design. One block — the memory arbiter — has bounded model
  checked properties; the arbiter between the CPU, the GPU and the only memory is
  exactly the kind of block where that pays.

## 7. Conclusion

`[TODO: your sentence. Mine would be: the design is ordinary; what is not ordinary
is that it says what it does not know.]`

## Reproducing this

```bash
git clone <repo> && cd Omni-RISC
./scripts/setup_demo.sh --check     # missing packages, and what provides them
./scripts/evidence.sh               # every number in this report
```

`RESULTS.md` is the per-claim version: what was measured, how, how to re-run it, and
what would falsify it.
