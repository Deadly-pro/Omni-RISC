# Omni-RISC APU — Architecture

## What this is

A CPU + SIMT-GPU SoC on FPGA (Xilinx Artix-7 XC7A100T). Solo build. The hardware-accelerated VFI work lives in the separate hls4ml project — this repo is the processor-architecture portfolio piece.

## Scope decision (July 2026)

The CPU is **RV32IM in-order 5-stage** (Path A). Out-of-order (ROB, rename, scoreboard, dual-issue, GShare) is deferred — not enough time before the September NVIDIA OA to finish it well, and a finished simple core beats a half-built OoO in interviews. Archived scope: `ooo-scope-archive` branch. Fixed-function ML accelerators (conv/upscale/VFI engines) were also cut — that territory belongs to the hls4ml project.

GPU tree (`hardware/rtl/gpu/`) keeps its planned scope but comes *after* the CPU boots firmware.

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

## Build order

1. **CPU** — pipeline -> hazards -> M-ext -> CSRs/traps -> passes rv32i + rv32im compliance
2. **SoC scaffolding** — AXI-Lite xbar + UART + CLINT, boot `hello.c` via UART
3. **Caches** — L1 I$ and D$ (direct-mapped first, upgrade if time)
4. **GPU** — SIMT frontend + one lane -> 4 lanes -> warp scheduler -> scratchpad -> kernels (vector_add, matmul, conv2d, relu)
5. **Synthesis** — Vivado on Artix-7, timing closure, utilization report

Steps past 3 are bonus. Interview-critical deliverable is step 1 + a clean writeup.
