# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**Omni-RISC APU** — RISC-V Accelerated Processing Unit targeting Xilinx Artix-7 (XC7A100T). Solo build, interview-driven timeline (Sept 2026):
- **CPU**: RV32IM, 5-stage in-order pipeline (IF/ID/EX/MEM/WB), forwarding + hazard detection, M-extension, M-mode CSRs/traps
- **GPU**: SIMT engine (4 warps × 4 lanes) with INT ALU and scratchpad — built after the CPU passes compliance
- **Bus**: AXI4-Lite interconnect

**Locked target — coherent APU** (July 2026): CPU and GPU share unified, consistent memory. Two scalar RV32IM cores with private L1s + **fine-grained snooping MSI** coherence between them; the GPU is a **coarse-grained** coherence participant (flush/invalidate + acquire/release at kernel boundaries), not a fine-grained snooping peer. Full phased plan + priority tiers in `docs/roadmap.md`.

**Scope guard**: OoO execution (ROB, rename, scoreboard, **dual-issue/superscalar**, branch prediction) is OUT — archived on `ooo-scope-archive`. Also OUT: MESI/write-back/4-core/full-directory coherence (MSI/write-through/2-core only), and MMU/virtual-memory/Linux (M-mode + bare-metal scheduler is the OS deliverable). Do not reintroduce or suggest these. Multicore coherence and coherent-APU integration are **stretch tiers gated behind a finished floor** (single core → compliance, GPU works, synthesis) — do not start them before the floor is done. Build order: A CPU→compliance · B SoC/UART/scheduler · C caches · D 2-core coherence · E GPU · F coherent-APU · G synth. See `docs/architecture.md` + `docs/roadmap.md`.

**Validation & physical design (July 2026 — target may shift)**: FPGA board access (Artix-7 XC7A100T) is now **uncertain**. Fallback validation path is **OpenROAD RTL-to-GDSII** on an open PDK (sky130): Yosys synth → place-and-route → OpenSTA timing → GDS, *not* Vivado FPGA synthesis. Verilator stays the functional sign-off; OpenROAD replaces the physical board as the "it's real silicon" evidence — an **upgrade** for the portfolio story (full physical-design credential > FPGA bitstream). RTL implications (note only, do not refactor pre-emptively): (1) the `*` multiplier maps to **standard cells**, not DSP48 hard blocks — still correct, but watch timing and be ready to pipeline it at a higher target clock; (2) `$readmemh` memories (`instr_bram.v`, `data_bram.v`) have no hard-BRAM equivalent — for ASIC they either synthesize behaviorally to flops (area-heavy, fine for validation) or swap to an **OpenRAM** macro, so keep memories behind a clean interface to localize that swap; (3) `make synth` (Vivado) remains for now — an OpenROAD flow becomes a parallel target, not a replacement.

## Common commands

All commands are driven through the root `Makefile`:

```bash
make sim TB=cpu/tb_alu       # Run a single Verilator testbench
make compliance              # Run riscv-arch-test rv32i/rv32im suite
make firmware                # Cross-compile bare-metal firmware (default APP=hello)
make synth                   # Vivado batch synthesis (hardware/vivado/synth.tcl)
make clean
```

Firmware app selection (from `firmware/`): `make APP=benchmark_cpu` — apps live in `firmware/apps/`.

Direct testbench invocation (more control than `make sim`):
```bash
./hardware/sim/run_sim.sh cpu/tb_alu --wave    # --wave opens GTKWave on the VCD
```

## How the simulation flow works

`hardware/sim/run_sim.sh` is the canonical build/run script. Key conventions it encodes — new RTL/testbenches must follow these or the script won't find them:

- Testbench C++ lives at `hardware/sim/<path>/tb_<name>.cpp`.
- DUT is auto-derived: `tb_<name>` → searches all of `hardware/rtl/**` for `<name>.v` or `<name>.sv`. So Verilog module filenames must match the DUT name exactly, and module name must match filename (`--top-module` is derived).
- All RTL subdirectories are added as `-I` include paths, so ``` `include ``` works across the tree.
- Verilator flags: `--cc --exe --trace --Wall -Wno-UNUSED -Wno-UNDRIVEN`. Build dir is per-testbench: `hardware/sim/obj_dir_<tb_basename>`.
- VCD traces write to `hardware/sim/waves/<tb_basename>.vcd`.

## Testbench conventions

- Existing TBs (`tb_alu`, `tb_regfile`, `tb_decoder`, `tb_cache`, soc TBs) define the module port names and encodings — **RTL must conform to the TB, not vice versa**. E.g. ALU opcode encoding (0=ADD … 9=SRA, 10=LUI_PASS, 11=AUIPC) is fixed by `tb_alu.cpp`.
- `tb_decoder` expects `immediate` as a decoder output — `imm_gen` is instantiated inside `decoder.v`.
- TBs exit 0 on pass, non-zero on fail; `run_sim.sh` reports PASS/FAIL from the exit code.

## Compliance testing

`tests/run_compliance.sh` runs the external `riscv-arch-test` suite. It assumes:
- The CPU sim binary lives at `hardware/sim/cpu/obj_dir/Vcpu_top` and accepts a `.hex` file as its first arg.
- The sim writes signature memory to `DUT-soc_top.signature` on exit for diffing against the reference.

Build the CPU top sim before running compliance. Record results in `docs/compliance_results.md`.

## Firmware toolchain

- Cross-compiler: `riscv32-unknown-elf-gcc` with `-march=rv32im -mabi=ilp32 -Os -ffreestanding -nostdlib`.
- Linker script: `firmware/boot/linker.ld`; entry via `boot/startup.S` + `boot/trap_handler.S` + `boot/boot.c`.
- Output: ELF → `objcopy -O verilog` → `.hex` for `$readmemh` in RTL memories (`hardware/rtl/memory/instr_bram.v`, `data_bram.v`).
- `scripts/elf_to_hex.py` for finer control over ELF → hex conversion.

## Repository layout (non-obvious pieces)

- `hardware/rtl/cpu/core/` — one module per pipeline stage (`fetch_stage`, `decode_stage`, `exec_stage`, `mem_stage`, `wb_stage`) plus datapath units (`alu`, `decoder`, `imm_gen`, `regfile`, `forwarding_net`, `branch_unit`, `multiplier`, `divider`, `lsu`, `pc_gen`) and control (`pipeline_ctrl`, `hazard_unit`, `csr_file`, `trap_unit`). `cpu_top.v` wires them together. Stub files carry a spec header (ports, widths, gotchas, done-when criteria) — implement to that spec.
- `hardware/rtl/gpu/core/` — SIMT frontend (`gpu_fetch`, `gpu_decode`, `warp_scheduler`) driving lanes (`exec_lane`, `gpu_alu`, `gpu_lsu`, `gpu_regfile`, `gpu_scratchpad`). `gpu_top.v` and `gpu_cmd_proc.v` sit above.
- `firmware/gpu_kernels/` — hand-written GPU assembly kernels (vector_add, matmul, conv2d, relu) for Phase 4.
- `docs/cpu_design.md` — the CPU spec including the 10-step bring-up order. Follow it; don't skip ahead.
- `docs/memory_map.md` — SoC address map; firmware drivers and the AXI xbar must agree with it.

## Requirements

Verilator ≥ 5.0, `riscv32-unknown-elf-gcc`, GTKWave, Vivado 2023.x for synth, Python 3.11 (numpy + pyelftools only).
