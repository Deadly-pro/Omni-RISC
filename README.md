# Omni-RISC APU

A RISC-V Accelerated Processing Unit: RV32IM CPU + SIMT GPU on FPGA (Xilinx Artix-7 XC7A100T). Solo build.

## Architecture

- **CPU**: RV32IM, 5-stage in-order pipeline (IF/ID/EX/MEM/WB), full forwarding + hazard detection, M-extension, M-mode CSRs + traps
- **GPU**: SIMT engine (4 warps × 4 lanes), INT ALU, scratchpad memory, warp scheduler with latency hiding
- **Bus**: AXI4-Lite interconnect (CPU ↔ peripherals ↔ GPU)
- **Target**: Xilinx Artix-7 (XC7A100T)

## Project Structure

```
hardware/rtl/cpu/     — RISC-V CPU core (5-stage pipeline, caches, CSRs)
hardware/rtl/gpu/     — SIMT GPU (warp scheduler, exec lanes, scratchpad)
hardware/rtl/bus/     — AXI4-Lite interconnect
hardware/rtl/memory/  — instruction/data BRAM
hardware/rtl/peripherals/ — UART, CLINT timer, GPIO
hardware/rtl/soc/     — top-level SoC integration
hardware/sim/         — Verilator testbenches + run_sim.sh driver
firmware/             — bare-metal RISC-V firmware (boot, drivers, apps, GPU kernels)
scripts/              — elf_to_hex.py (ELF → $readmemh)
tests/                — riscv-arch-test compliance runner, GPU kernel tests
docs/                 — architecture and design docs (read cpu_design.md first)
```

## Build Phases

| Phase | What | Status |
|-------|------|--------|
| 1 | CPU core: pipeline → hazards → M-ext → CSRs/traps → compliance | 🔨 in progress |
| 2 | SoC: AXI-Lite bus, UART, CLINT — boot hello.c | 🔲 |
| 3 | L1 caches | 🔲 |
| 4 | GPU: SIMT engine, kernels, benchmarks | 🔲 |
| 5 | FPGA synthesis + timing closure on Artix-7 | 🔲 |

## Quick Start

```bash
# Run a single testbench
make sim TB=cpu/tb_alu
# or directly, with waveforms:
./hardware/sim/run_sim.sh cpu/tb_alu --wave

# Cross-compile firmware
make firmware              # default APP=hello

# RISC-V compliance suite (needs CPU top sim built first)
make compliance
```

## Requirements

- Verilator >= 5.0
- riscv32-unknown-elf-gcc
- GTKWave (waveforms)
- Vivado 2023.x (synthesis only)
- Python 3.11 (`pip install -r requirements.txt`)
