# Omni-RISC APU

A **RISC-V Accelerated Processing Unit** — an RV32IM scalar CPU fused with a SIMT GPU on a single FPGA. Solo build, verified against the official RISC-V compliance suite, synthesized on Xilinx Artix-7 (XC7A100T).

![Omni-RISC APU architecture](docs/diagrams/architecture.png)

> Diagram generated from the RTL module hierarchy (`scripts/gen_arch_diagram.py`): solid edges are module instantiations, dashed edges are coherence relationships.

## Highlights

- **RV32IM CPU** — 5-stage in-order pipeline (IF/ID/EX/MEM/WB), full forwarding + hazard detection, M-extension (`mul`/`div`), M-mode CSRs + traps
- **Coherent 2-core research rig** — snooping MSI D-cache (write-through, write-invalidate), LR/SC spinlock, passed MP + SB litmus tests
- **SIMT GPU** — 4 warps × 4 lanes, 16-bit SIMT ISA, warp scheduler with latency hiding, scratchpad, vector/matmul/conv kernels
- **SoC** — UART, timer/CLINT, GPIO, pbus MMIO decode, bare-metal firmware + scheduler
- **Fits on the board** — synthesis/implementation on XC7A100T: **7,516 LUTs (11.9%) incl. 1,580 LUT-as-memory, 2,410 FF, 64.5 BRAM tiles, 15 DSP; 50 MHz timing met (WNS +0.685ns) → Fmax ≈ 51.8 MHz**
- **Compliance** — riscv-arch-test rv32im: **53/54** (1 skip, Zifencei `fence.i`: split I/D BRAMs)

## Interactive demo — a shell on the APU

You can type into a FreeRTOS shell running on the RV32IM core, inside the Verilator simulation, over a bit-accurate UART (`tb_soc_shell` feeds stdin at real 115200 framing — 434 clk/bit, no zero-delay cheat):

![Interactive FreeRTOS shell session](docs/shell_demo.gif)

```bash
./tests/test_shell.sh        # scripted session gate: help/uptime/ps/ticks/gpu/quit
```

The `gpu` command writes A/B vectors into the GPU scratchpad through the coherent host window, launches the vector-add kernel over MMIO, polls until the warp halts, and prints the checksum. The session is replayable with `asciinema play docs/shell_session.cast`.

## Architecture

- **CPU**: RV32IM, 5-stage in-order pipeline, full forwarding + hazard detection, M-extension, M-mode CSRs + traps
- **Coherence rig** (`dual_core_top`): two cores sharing one `data_bram` through a snooping MSI D-cache and pbus arbiter — the coherence experiments that the APU's coarse-grained GPU sync builds on
- **GPU**: SIMT engine (4 warps × 4 lanes), INT ALU, scratchpad memory, warp scheduler with latency hiding
- **CPU↔GPU shared-memory window**: the CPU writes kernel inputs into the warp's scratchpad and reads results back through `gpu_cmd_proc` host registers — no copies, coarse-grained coherence at kernel boundaries
- **FreeRTOS V10.5.1** — preemptive RTOS on the core (M-mode CLINT port): measured 1.8 µs ISR latency, 9.8 µs context switch, interactive UART shell

Full hardware/software architecture: [`docs/architecture.md`](docs/architecture.md)
- **Bus**: peripheral bus (pbus) with MMIO decode — CPU ↔ UART/timer/GPIO ↔ GPU
- **Target**: Xilinx Artix-7 (XC7A100T), Vivado 2026.1 synthesis flow

## Verification

| Area | Result |
|------|--------|
| ISA compliance | riscv-arch-test rv32im 53/54 |
| CPU pipeline | tb_cpu_top 83/83 |
| L1 caches | tb_cache 14/14, tb_icache 27/27 |
| MSI coherence | tb_dual_spin (counter → 1000, no lost updates), litmus MP + SB 200/200 each |
| GPU kernels | tb_gpu_top_kernels 66/66 (vector_add, relu, matmul, conv2d) |
| SoC end-to-end | tb_soc_gpu: CPU writes A/B into GPU scratchpad, launches vector-add kernel over pbus, reads C back — `C = A + B` per lane |
| FreeRTOS | tb_soc_rtos: real preemptive RTOS (V10.5.1, M-mode port) — two tasks preempt correctly, tick timing cycle-accurate |
| RTOS metrics | ISR latency 92 cyc avg (1.84 µs), 2-switch yield round trip 492 cyc (9.84 µs), tick jitter ±194 cyc over 1200+ ticks |
| Interactive shell | tb_soc_shell + tests/test_shell.sh: typed FreeRTOS shell over honest-baud UART, GPU kernel launched from a shell command |

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
docs/                 — architecture.md (start here), gpu_design, memory_map, compliance results
```

## Quick Start

```bash
# Build + run a single testbench (Verilator)
make sim TB=cpu/tb_alu
# or directly, with waveforms:
./hardware/sim/run_sim.sh cpu/tb_alu --wave

# Cross-compile firmware
make firmware              # default APP=hello

# RISC-V compliance suite (53/54)
make compliance

# Vivado synthesis + implementation (place+route)
make synth                 # needs Vivado on PATH or make VIVADO=...
make impl
```

## Requirements

- Verilator >= 5.0
- riscv64-linux-gnu-gcc (rv32 multilib)
- GTKWave (waveforms)
- Vivado 2026.1 (synthesis/implementation; override path with `make VIVADO=...`)
- Python 3 (`pip install -r requirements.txt`)

## License

Apache-2.0 — see [LICENSE](LICENSE).

## References

- RISC-V ISA Manual
- riscv-arch-test (official compliance suite)
