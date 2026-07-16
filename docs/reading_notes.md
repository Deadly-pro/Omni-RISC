# Reading Notes — Omni-RISC APU

Primary plan: `revision.md` in repo root (10-day day-by-day). This file is for *sources*, not schedule.

## RISC-V ISA

- **RISC-V Unprivileged ISA Spec v20191213** — Chapter 2 (RV32I) and Chapter 7 (RV32M). Read cover-to-cover once; keep open during decoder work.
  https://riscv.org/technical/specifications/
- **RISC-V Privileged Spec v20211203** — Chapter 3 only (Machine-level ISA: mstatus, mtvec, mepc, mcause, traps). Skip S/U mode entirely.
- **Green Card** (1-page reference) — print, tape next to keyboard.
  https://inst.eecs.berkeley.edu/~cs61c/resources/RISCV_Reference_Card.pdf

## CPU (in-order 5-stage, hazards, M-ext, traps)

- **Patterson & Hennessy — Computer Organization and Design (RISC-V Edition), 2nd ed** — Chapter 4 sections 4.5–4.8. This IS the reference for the CPU build.
- **Berkeley CS61C lectures 20–23** (YouTube) — same material, tighter, free.
  https://inst.eecs.berkeley.edu/~cs61c/
- **Berkeley Sodor 5-stage** — cleanest reference implementation. Read `dpath.scala` + `cpath.scala` for style; do not copy.
  https://github.com/ucb-bar/riscv-sodor/tree/master/src/main/scala/rv32_5stage
- **darkriscv** — small Verilog RV32I core. Useful when stuck on how to wire a specific stage register.
  https://github.com/darklife/darkriscv
- **PicoRV32** — production-quality RV32I in Verilog. Overkill to read cover-to-cover; use as sanity check.
  https://github.com/YosysHQ/picorv32

## GPU / SIMT (for the accelerator + interview signal)

The classic gap: undergrad architecture books teach CPU, not GPU. These are the sources that actually work:

- **NVIDIA CUDA C Programming Guide** — Chapter 5 (Performance Guidelines) and Appendix on compute capability. Best free source for warp/SIMT/coalescing/occupancy from the horse's mouth.
  https://docs.nvidia.com/cuda/cuda-c-programming-guide/
- **PMPP — Programming Massively Parallel Processors, 4th ed (Kirk, Hwu, Hajj)** — THE textbook for GPU architecture at the undergrad+ level. Chapters 1–6 are the core; chapters 7–10 for memory hierarchy and coalescing. If you buy one book for the GPU side, buy this.
- **GPGPU-Sim manual + papers** — architecture details of a modeled NVIDIA-like GPU (SM, warp scheduler, memory subsystem). More rigorous than any tutorial.
  https://gpgpu-sim.org/manual/
- **Hennessy & Patterson — Computer Architecture: A Quantitative Approach, 6th ed**, Chapter 4 (Data-Level Parallelism, GPUs vs vector processors). One chapter, dense, worth it.
- **NVIDIA whitepapers** — Volta (V100), Turing, Ampere (A100), Hopper (H100), Ada. Free PDFs. Read Volta first (introduces independent thread scheduling), then Ampere (TF32, sparsity, MIG). These are exactly what interviewers assume you've seen.
  https://www.nvidia.com/en-us/data-center/resources/
- **Vasily Volkov's Berkeley thesis "Understanding Latency Hiding on GPUs"** (2016) — the paper that reframes occupancy. Not a tutorial; a mental model reset.
  https://www2.eecs.berkeley.edu/Pubs/TechRpts/2016/EECS-2016-143.html
- **"GPUs Explained" by Fabian Giesen** (blog series, ryg) — the readable version of what a modern GPU actually does, written by someone who worked on the RAD/Epic side.
  https://fgiesen.wordpress.com/2011/07/09/a-trip-through-the-graphics-pipeline-2011-index/
- **Simty (INRIA) — synthesizable SIMT core in Verilog** — closest thing to reference RTL for what you're building on the GPU side.
  https://team.inria.fr/pacap/software/simty/

**Reading order for GPU:** PMPP Ch 1–4 → CUDA Programming Guide Ch 5 → Volta whitepaper → PMPP Ch 5–6 → Ampere whitepaper → Volkov thesis → then Simty when you start RTL.

## Verilog / SystemVerilog

- **HDLBits** — practice, not reading. Skip if 4-core project is fresh.
  https://hdlbits.01xz.net/
- **"Verilog Coding Styles for Improved Simulation Efficiency" — Cliff Cummings SNUG papers** — the blocking/non-blocking rules you must never break.
  http://www.sunburst-design.com/papers/
- **Verilator docs** — required for the sim flow this repo uses.
  https://verilator.org/guide/latest/

## Compliance / verification

- **riscv-arch-test** — the suite `tests/run_compliance.sh` invokes. Read the README once so you understand the signature-diff model.
  https://github.com/riscv-non-isa/riscv-arch-test
- **RISC-V Formal Interface (riscv-formal)** — if you get compliance passing early and want the harder verification story for the interview.
  https://github.com/YosysHQ/riscv-formal

## What to skip (for now)

- Anything on OoO, ROB, Tomasulo, register renaming — not this scope.
- Cache coherence protocols beyond MESI overview.
- SystemVerilog assertions (SVA) — nice-to-have, not blocking.
- Any FPGA-vendor-specific IP guides. Vivado synthesis comes at the end.
