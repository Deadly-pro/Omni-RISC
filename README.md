# Omni-RISC APU

A RISC-V Accelerated Processing Unit: RV32IM CPU + SIMT GPU on FPGA (Xilinx Artix-7 XC7A100T). Solo build.

## Architecture

- **CPU**: RV32IM, 5-stage in-order pipeline (IF/ID/EX/MEM/WB), full forwarding + hazard detection, M-extension, M-mode CSRs + traps
- **GPU**: SIMT engine (4 warps × 4 lanes), INT ALU, scratchpad memory, warp scheduler with latency hiding
- **Bus**: peripheral bus (pbus) with MMIO decode — CPU ↔ UART/timer/GPIO ↔ GPU (AXI4-Lite is the documented future upgrade)
- **Target**: Xilinx Artix-7 (XC7A100T), Vivado 2026.1 synthesis flow

## Project Structure

```
hardware/rtl/cpu/     — RISC-V CPU core (5-stage pipeline, caches, CSRs, 2-core MSI)
hardware/rtl/gpu/     — SIMT GPU (warp scheduler, exec lanes, scratchpad)
hardware/rtl/memory/  — instruction/data BRAM
hardware/rtl/soc/     — UART, CLINT timer, GPIO, SoC integration (soc_top, dual_core_top)
hardware/rtl/bus/     — pbus_arbiter (2-core arbitration)
hardware/sim/         — Verilator testbenches + run_sim.sh driver
firmware/             — bare-metal RISC-V firmware (boot, drivers, apps, GPU kernels)
scripts/              — elf_to_hex.py, gpu_asm.py (16-bit SIMT assembler)
tests/                — riscv-arch-test compliance runner, GPU kernel tests
docs/                 — architecture and design docs (read roadmap.md first)
```

## Build Phases

| Phase | What | Status |
|-------|------|--------|
| A | CPU core: pipeline → hazards → M-ext → CSRs/traps → compliance (53/54) | ✅ |
| B | SoC: UART, timer/CLINT, GPIO, firmware, bare-metal scheduler | ✅ |
| C | L1 caches (I$ direct-mapped, D$ 2-way write-through) | ✅ |
| D | 2-core snooping MSI coherence, LR/SC spinlock, litmus | ✅ |
| E | SIMT GPU: scheduler, lanes, kernels, SoC integration | ✅ |
| F | Coherent-APU integration (unified memory, flush/invalidate) | 🔧 in progress |
| G | Synthesis + timing closure on Artix-7 (Vivado) | ✅ |

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
- riscv64-linux-gnu-gcc (rv32 multilib)
- GTKWave (waveforms)
- Vivado 2026.1 at /tools/2026.1/Vivado (synthesis/implementation)
- Python 3 (`pip install -r requirements.txt`)
