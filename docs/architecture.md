# Omni-RISC APU — Architecture

## What this is

A CPU + SIMT-GPU SoC on FPGA (Xilinx Artix-7 XC7A100T). Solo build. The hardware-accelerated VFI work lives in the separate hls4ml project — this repo is the processor-architecture portfolio piece.

## Scope decision (July 2026, updated)

The CPU core is **RV32IM in-order 5-stage** (Path A). Out-of-order (ROB, rename, scoreboard, **dual-issue/superscalar**, GShare) stays deferred — a finished simple core beats a half-built OoO. Archived scope: `ooo-scope-archive` branch. Fixed-function ML accelerators (conv/upscale/VFI engines) were cut — that belongs to the hls4ml project.

**Locked target model — a coherent APU.** CPU and GPU share **unified, consistent memory**. Two scalar RV32IM cores with private L1s and **fine-grained snooping MSI** coherence between them; the GPU is a **coarse-grained** coherence participant (flush/invalidate + acquire/release at kernel boundaries), *not* a fine-grained snooping peer. MSI/write-through/2-core on purpose — MESI/write-back/4-core/full-directory is the months-trap and is out. Full plan, phases, and priority tiers: **`docs/roadmap.md`**.

The 2-core coherence and coherent-APU integration are **stretch tiers gated behind a finished floor** (single core → compliance, GPU works, synthesis). GPU tree (`hardware/rtl/gpu/`) keeps its planned scope but comes *after* the CPU boots firmware.

## Block diagram

```
      +-----------------------------------+
      |       AXI4-Lite Interconnect      |
      +---+----------+----------+---------+
          |          |          |
      +---v---+  +---v---+  +---v---+
      | RV32IM|  | SIMT  |  | PERIPH|
      |  CPU  |  |  GPU  |  | (UART,|
      | 5-stg |  | 4W x  |  | GPIO, |
      |       |  |  4L   |  | CLINT)|
      +---+---+  +---+---+  +-------+
          |          |
      +---v----------v---+
      |  L1$ / BRAM      |
      +------------------+
```

## Build order (phases — see `docs/roadmap.md` for full detail)

- **A. CPU core → compliance** *(floor, in progress)* — pipeline -> forwarding -> hazards -> M-ext -> CSRs/traps -> rv32i+rv32im compliance
- **B. SoC scaffolding** *(high value)* — AXI-Lite xbar + UART + CLINT, boot `hello.c`, bare-metal trap-driven scheduler
- **C. L1 caches** *(stretch prereq)* — I$ + D$ (D$ write-through, to simplify coherence)
- **D. 2-core snooping MSI coherence** *(stretch)* — replicate core, shared bus, MSI, LR/SC, litmus tests
- **E. SIMT GPU** *(floor)* — frontend + lane -> 4 lanes -> warp scheduler -> scratchpad -> kernels
- **F. Coherent-APU integration** *(stretch capstone)* — unified memory, coarse-grained CPU↔GPU coherence, acquire/release at kernel boundaries
- **G. Synthesis + measurement** *(floor)* — Vivado on Artix-7, timing, utilization, CPI, kernel speedup

Floor = A + E + G. Stretch (C→D→F) is gated behind the floor.
