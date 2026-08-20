# AGENTS.md

Development notes and verification conventions for this repository.

## Project

**Omni-RISC APU** — RISC-V Accelerated Processing Unit (RV32IM CPU + SIMT GPU), solo build. Target: coherent APU where two scalar RV32IM cores share coherent memory via **fine-grained snooping MSI** (write-through, write-invalidate, 2 agents only) and the GPU is a **coarse-grained** coherence participant (flush/invalidate + acquire/release at kernel boundaries). See `docs/roadmap.md` (locked plan, priority tiers) and `docs/architecture.md`.

**Status (Aug 2026)** — the FLOOR is nearly done; do not treat old "Phase A in progress" docs as current:
- **A CPU→compliance: DONE** — riscv-arch-test 53/54 (1 skip, Zifencei-fence.i: split I/D BRAMs make self-modifying code impossible). Results in `docs/compliance_results.md`.
- **B SoC/firmware: DONE** — UART, timer/CLINT, GPIO, pbus decode, bare-metal scheduler. Caches wired in behind `USE_CACHES` (default 0; SoC runs cacheless).
- **C L1 caches: DONE** — `l1_dcache` 4KB 2-way write-through (tb_cache 14/14), `l1_icache` 4KB direct-mapped (tb_icache 27/27).
- **D 2-core coherence: DONE** — snooping MSI, LR/SC spinlock (tb_dual_spin: counter→1000, no lost updates), litmus MP+SB 200/200 each.
- **E SIMT GPU: DONE** — 7/7 TBs, tb_gpu_top_kernels 66/66 (vector_add/relu/matmul/conv2d across 4 warps).
- **F coherent-APU: DONE** — shared-memory window: CPU writes kernel inputs into warp0's 4-bank scratchpad via `HOST_WIN`/`HOST_DATA` (`0x4000_2018`/`0x4000_201C`), launches over MMIO, polls `STATUS` (`+0x20`, low nibble = active_warps) to zero (release), reads results back over the same window (acquire). `tb_soc_gpu` now runs a real vector-add: CPU writes A/B → kernel computes C → readback verified `C = A + B` per lane. Remaining (deferred): flush/invalidate of GPU-facing data and a full unified kernel memory (kernel image is still flashed into IMEM at synth time, not pushed over pbus).
- **G synth+measure: DONE** — `make synth` + `impl.tcl` green on Vivado 2026.1. **Synth: 6,717 LUTs (10.6%), 1,584 LUT-as-memory, 2,379 FF, 64.5 BRAM tiles (47.8%), 15 DSP. Impl (place+route): 7,516 LUTs (11.9%), 2,410 FF, 64.5 BRAM, 15 DSP; WNS +0.823ns @50MHz → Fmax ≈ 52.6 MHz** (critical path `mem_wb_rdata` → EX ALU result, 14 logic levels incl. 3 DSP48E1). Bitstream off (no board).
- **H FreeRTOS: DONE (scheduler up)** — FreeRTOS V10.5.1 (vendored under `firmware/third_party/FreeRTOS`, M-mode CLINT port) boots on the SoC: `tb_soc_rtos` PASS — task A (prio 2) ticks 1-4 at exactly 25.05M cycles apart (500ms @ 50MHz), task B (prio 1) prints at 60.09M (1200ms), preemption correct. The bring-up exposed a real core bug (`load;load;branch` mis-evaluated because `decode_stage` gave the hazard `stall` priority over the pipeline-freeze `hold`, erasing a frozen load's `mem_read`) — fixed, full regression green; see `docs/rtos_bringup.md` + `tests/rtos/load_branch_bug.md`. Remaining: queue/ISR-semaphore phases, scheduler metric numbers (context-switch cost, tick jitter, ISR latency).
  - **Warning on the OLD numbers:** the prior "6,270 LUTs / 10,720 FF / 18 DSP / WNS +1.305" was an **artifact** — the old `gpu_demo.S` was an all-constant kernel (LDI/ADD chain → `sp[0]=42`), so Vivado constant-folded the entire GPU datapath AND its 128Kb of storage (regfile + scratchpad), and "impl" was never actually run (that WNS was the synth timing report). The numbers above are the first honest full-APU measurement with a real data-dependent kernel (vector-add reads A/B from scratchpad).
  - **GPU memories must stay RAM-inferable:** `gpu_regfile` and `gpu_scratchpad` have **no reset** (BRAM/LUTRAM can't be reset) and the scratchpad uses **one write port per bank** (host write shares the lane port with host priority — the coherence protocol guarantees write-before-read, no collision). Re-adding a reset loop or a second write port flattens them to FFs/muxes → GPU explodes to ~54K LUTs (regfile 35,841 + scratchpad 17,474 + 32,768 FF).
  - `data_bram` needs `(* ram_decomp = "power" *)`: the 256KB array cascades 64 RAMB36E1s deep and otherwise trips place_design DRC **REQP-1962** (ADDRARDADDR[15] tie-off mismatch in the cascade chain).

**Scope guard (do not reintroduce):** OoO/ROB/rename/scoreboard/dual-issue/branch-predict (archived on `ooo-scope-archive`); MESI/write-back/4-core/directory coherence (MSI/write-through/2-core only); MMU/virtual-memory/Linux (M-mode + bare-metal scheduler only).

**Physical design (Aug 2026):** Vivado is THE flow from now on — v2026.1 at `/tools/2026.1/Vivado/bin/vivado` (the OpenROAD/sky130 fallback is dropped). FPGA board access is still uncertain, so synthesis/implementation reports are the evidence until a board appears. Hard-won RTL lessons for Vivado bring-up (Verilator tolerates these, Vivado rejects):
- `reg`-declared signals driven by `assign` or by a module output port are illegal — fixed in `timer.v`, `exec_stage.v`, `pc_gen.v`, `decode_stage.v`; array output ports are illegal too (`warp_scheduler` `debug_pc` removed). After any such fix, re-run the affected sims (tb_cpu_top 83/83, tb_soc_gpu, tb_dual_spin etc. — they are the regression gate).
- `make synth` stages the REAL firmware hex (`benchmark_gpu.hex` → `program.hex`, `gpu_demo.hex`); an empty/stub hex constant-folds the whole design to 0 LUTs and is NOT a valid measurement.
- **Registered read = BRAM (Phase G):** `data_bram`'s read was combinational, forcing its 256KB array into distributed RAM (32,768 LUTs = 172% of LUT-memory capacity → place_design DRC UTLZ-1). Registering the read (`rdata <= mem[addr[17:2]]`) maps it to BRAM (LUT-mem 0, 64 RAMB36E1). This changes load timing — the pipeline needs a 1-cycle `mem_hold` per data load (`mem_stage` `dbram_hold` toggle) and level-req handshakes must be acked (`dc_mem_ack`/`core*_read_ack` toggles). **Critical follow-on bug:** a branch/jump frozen in EX by a hold had its own `redirect_valid` flush its ID/EX bundle before EX/MEM captured it (link value lost → `tb_soc_gpu` prints "GPU(4444…"); fixed by giving `hold` priority over `flush` in `decode_stage`.
- `$readmemh` BRAMs have no hard-ASIC equivalent — keep memory ports behind a clean interface so an OpenRAM swap stays localized.

## Common commands

All driven through the root `Makefile`:

```bash
make sim TB=cpu/tb_alu       # Run one Verilator testbench (any hardware/sim/<path>/tb_<name>.cpp)
make compliance              # riscv-arch-test rv32im suite (builds tb_compliance; see below)
make firmware APP=hello      # Cross-compile bare-metal firmware (default APP=hello)
make synth                   # Vivado batch synthesis (hardware/vivado/synth.tcl)
make clean
```

Firmware apps live in `firmware/apps/`: `make APP=benchmark_cpu`, `APP=benchmark_gpu`, `APP=timer`.

Direct testbench invocation (more control than `make sim`):
```bash
./hardware/sim/run_sim.sh cpu/tb_alu --wave    # --wave opens GTKWave on the VCD
```

## Simulation flow

`hardware/sim/run_sim.sh` is the canonical build/run script; new RTL/TBs must follow these conventions or it won't find them:

- TB C++ lives at `hardware/sim/<path>/tb_<name>.cpp`. DUT auto-derived as `<name>` → searched in `hardware/rtl/**` as `<name>.v`/`.sv`. **Module filename must match module name and DUT name** (`--top-module` is derived).
- **Special DUT mappings baked into the script** (TB name overrides auto-derive):
  `tb_soc_*`→`soc_top`, `tb_cache`→`l1_dcache`, `tb_icache`→`l1_icache`, `tb_compliance`→`cpu_top`, `tb_dual_*`/`tb_litmus_*`→`dual_core_top`, `tb_gpu_top_*`→`gpu_top`.
  Caveat: `tb_lrsc`, `tb_2store`, `tb_cpu2` instantiate `cpu_top`/`cpu2_top` but have **no mapping** — they're built from existing `obj_dir_*` trees, not a fresh `make sim`.
- Verilator flags: `--cc --exe --trace --Wall -Wno-UNUSED -Wno-UNDRIVEN`; build dir `hardware/sim/obj_dir_<tb>`, VCD → `hardware/sim/waves/<tb>.vcd`. All RTL dirs are `-I` include paths.
- `run_sim.sh` auto-builds staging hex for `tb_dual_spin` (spinlock.S) and `tb_soc_gpu` (`firmware` APP=benchmark_gpu + `gpu_asm.py`); for `tb_soc_gpu` the DUT memory loads `program.hex` via `$readmemh`.

## Testbench conventions

- Existing TBs (`tb_alu`, `tb_regfile`, `tb_decoder`, `tb_cache`, soc/cpu_top/dual_core TBs) define module port names and encodings — **RTL must conform to the TB, not vice versa**. E.g. ALU opcode encoding (0=ADD … 9=SRA, 10=LUI_PASS, 11=AUIPC) is fixed by `tb_alu.cpp`; `tb_decoder` expects `immediate` as a decoder output (`imm_gen` instantiated inside `decoder.v`).
- TBs exit 0 on pass, non-zero on fail; `run_sim.sh` reports PASS/FAIL from the exit code.

## Compliance testing

`tests/run_compliance.sh` drives the external `riscv-arch-test` suite. Assumptions (verified — do not reuse older docs here):
- DUT harness: **`hardware/sim/obj_dir_tb_compliance/Vcpu_top`** — build it first via `./hardware/sim/run_sim.sh cpu/tb_compliance`.
- The harness is arg-driven: `+hex=<test.hex> +entry=0x100000 +pass=<..> +fail=<..> +tohost=<..> +sig=<..> +words=<..> +out=<file>`. No hardcoded signature filename.
- **Reference model is NOT Sail/Spike**: qemu-user semihosting can't extract signatures, so the golden signatures come from `tests/omni-risc/ref_sim.cpp`, a self-contained RV32IM(+Zicsr) interpreter written from the ISA spec. Per-test `.dut.sig` vs `.ref.sig` are diffed under `tests/compliance_work/`.
- Runner uses `riscv64-linux-gnu-gcc` (rv32 multilib, `-march=rv32im_zicsr_zifencei`). Only Zifencei-fence.i is skipped (split I/D BRAMs). Record results in `docs/compliance_results.md`.

## Firmware toolchain

- Cross-compiler: **`riscv64-linux-gnu-gcc`** (rv32 multilib) — the `riscv32-unknown-elf-*` triple is NOT installed. Override with `make PREFIX=...` if needed.
- Flags: `-march=rv32im_zicsr_zifencei -mabi=ilp32 -Os -ffreestanding -nostdlib`; linker script `firmware/boot/linker.ld`; entry via `boot/startup.S` + `trap_handler.S` + `boot.c`.
- Flow: ELF → `objcopy -O binary` → `firmware/elf_to_hex.py` → `.hex` for `$readmemh` (`scripts/elf_to_hex.py` for finer control). `make dump` produces a disassembly.
- **All `*.hex` are gitignored build artifacts** — regenerate after a fresh clone or `make clean`.

## GPU tooling

- `scripts/gpu_asm.py` assembles the 16-bit SIMT ISA `.S` kernels → `$readmemh` `.hex` (op[15:12] rd rs1 rs2 f3 encoding; see header).
- Kernel sources: `firmware/gpu_kernels/*.S`; the matching `.hex` files are **not tracked** — run `gpu_asm.py` before `tb_gpu_top_kernels` (it loads `firmware/gpu_kernels/<name>.hex`).
- GPU kernel reference data: `python3 tests/test_gpu_kernels.py` generates the `.hex` vectors the RTL TBs compare against.

## Vivado synthesis

- `make synth` = build `benchmark_gpu` firmware + assemble `gpu_demo.hex`, then `vivado -mode batch -source hardware/vivado/synth.tcl` (top `soc_top`, part `xc7a100tcsg324-1`, 50 MHz from `hardware/constraints/artix7.xdc`). Reports → `hardware/vivado/reports/{synth_utilization,synth_timing}.rpt`, netlist → `synth.dcp`.
- `synth.tcl` stages the real firmware into the run dir as `program.hex`/`gpu_demo.hex`; results are only meaningful with a non-empty image (see the constant-fold note above).
- Implementation (place+route → Fmax, bitstream): `vivado -mode batch -source hardware/vivado/impl.tcl` (requires `synth.dcp`; proven green, see Phase G numbers above; `write_bitstream` needs real pin constraints in the xdc).

## Repository layout (non-obvious pieces)

- `hardware/rtl/cpu/core/` — one module per stage (`fetch/decode/exec/mem/wb_stage`), datapath (`alu`, `decoder`, `imm_gen`, `regfile`, `forwarding_net`, `branch_unit`, `multiplier`, `divider`, `lsu`, `pc_gen`), control (`pipeline_ctrl`, `hazard_unit`, `csr_file`, `trap_unit`); `cpu_top.v` wires them. Stub files carry a spec header — implement to that spec. Multicore: `cpu2_top.v` (2 cores + pbus arbiter), `dual_core_top.v` (shared data_bram + snoop bus, see `docs/roadmap.md` Phase D).
- `hardware/rtl/cpu/cache/` — `l1_dcache.v`, `l1_icache.v`, `l1_dcache_msi.v`. Gated by `USE_CACHES` param on `cpu_top` (0=direct BRAMs for tb_cpu_top/compliance, 1=cached SoC). `mem_stage` has `SHARED_MEM` param (1 = dcache backing memory external, dual-core).
- `hardware/rtl/gpu/` — `gpu_top.v` (pbus slave) + `gpu_cmd_proc.v` over `core/` (`gpu_fetch`, `gpu_decode`, `warp_scheduler`, `exec_lane`, `gpu_alu`, `gpu_lsu`, `gpu_regfile`, `gpu_scratchpad`).
- `hardware/rtl/soc/` — live UART/timer(CLINT)/GPIO + `soc_top.v`. (The old `hardware/rtl/peripherals/` and the unused AXI bus modules in `hardware/rtl/bus/` were deleted as dead code; only `bus/pbus_arbiter.v` remains, used by `dual_core_top`.)
- `tests/omni-risc/` — compliance reference interpreter (`ref_sim.cpp`) + link/macros. `tests/riscv-arch-test/` and `tests/compliance_work/` are cloned/generated (gitignored).
- `docs/memory_map.md` — SoC address map; firmware drivers and the pbus decode must agree with it. `docs/cpu_design.md` — pipeline spec + 10-step bring-up order (historical; phases done).

## Requirements

Verilator ≥ 5.0, `riscv64-linux-gnu-gcc` (rv32 multilib), GTKWave, Vivado 2026.1 at `/tools/2026.1/Vivado` (synth/impl only), Python 3 (numpy + pyelftools via `requirements.txt`).
