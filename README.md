# Omni-RISC APU

A RISC-V Accelerated Processing Unit: 2-wide dual-issue out-of-order CPU + SIMT GPU accelerator on FPGA.

## Architecture

- **CPU**: RV32IM, 7-stage pipeline, dual-issue OoO (scoreboard-based), GShare branch predictor, 16-entry ROB
- **GPU**: SIMT engine (4 warps × 4 lanes), INT8/16/32 ALU, scratchpad memory
- **Bus**: AXI4-Lite interconnect (CPU ↔ peripherals ↔ GPU)
- **Target**: Xilinx Artix-7 (XC7A100T)

## Project Structure

```
hardware/rtl/cpu/     — RISC-V CPU core (pipeline, caches, CSRs)
hardware/rtl/gpu/     — SIMT GPU accelerator
hardware/rtl/bus/     — AXI4 interconnect
hardware/rtl/soc/     — Top-level SoC integration
hardware/sim/         — Verilator testbenches
firmware/             — Bare-metal RISC-V firmware
model/                — Python ML models (VFI, FSRCNN)
scripts/              — Utility scripts (hex conversion, etc.)
tests/                — Test infrastructure
docs/                 — Architecture documentation
```

## Build Phases

| Phase | What | Status |
|-------|------|--------|
| 0 | Environment + Reading | 🔲 |
| 1 | CPU Core (Layer 0→5: scalar → OoO) | 🔲 |
| 2 | SoC (Bus, UART, Timer, Caches) | 🔲 |
| 3 | GPU Core (SIMT Engine) | 🔲 |
| 4 | GPU Maturation (Kernels, DMA) | 🔲 |
| 5 | VFI Inference Workload | 🔲 |

## Quick Start

```bash
# Run a testbench
cd hardware/sim
./run_sim.sh cpu/tb_alu

# Cross-compile firmware
cd firmware
make

# Run compliance tests
cd tests
./run_compliance.sh
```

## Requirements

- Verilator >= 5.0
- riscv32-unknown-elf-gcc
- Python 3.11 + PyTorch 2.x
- Vivado 2023.x (for FPGA synthesis)
- GTKWave (waveform viewing)
