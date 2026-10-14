# AGENTS.md

Development notes and verification conventions for this repository.

## Project

**Omni-RISC APU** — RISC-V Accelerated Processing Unit (RV32IM CPU + SIMT GPU), solo build. Target: coherent APU where two scalar RV32IM cores share coherent memory via **fine-grained snooping MSI** (write-through, write-invalidate, 2 agents only) and the GPU is a **coarse-grained** coherence participant (flush/invalidate + acquire/release at kernel boundaries). See `docs/history/roadmap.md` (locked plan, priority tiers) and `docs/architecture.md`.

**Status (Aug 2026)** — the FLOOR is nearly done; do not treat old "Phase A in progress" docs as current:
- **A CPU→compliance: DONE** — riscv-arch-test 53/54 (1 skip, Zifencei-fence.i: split I/D BRAMs make self-modifying code impossible). Results in `docs/compliance_results.md`.
- **B SoC/firmware: DONE** — UART, timer/CLINT, GPIO, pbus decode, bare-metal scheduler. Caches wired in behind `USE_CACHES` (default 0; SoC runs cacheless).
- **C L1 caches: DONE** — `l1_dcache` 4KB 2-way write-through (tb_cache 14/14), `l1_icache` 4KB direct-mapped (tb_icache 27/27).
- **D 2-core coherence: DONE** — snooping MSI, LR/SC spinlock (tb_dual_spin: counter→1000, no lost updates), litmus MP+SB 200/200 each.
- **E SIMT GPU: DONE** — 7/7 TBs, tb_gpu_top_kernels 66/66 (vector_add/relu/matmul/conv2d across 4 warps).
- **F coherent-APU: DONE** — shared-memory window: CPU writes kernel inputs into warp0's 4-bank scratchpad via `HOST_WIN`/`HOST_DATA` (`0x4000_2018`/`0x4000_201C`), launches over MMIO, polls `STATUS` (`+0x20`, low nibble = active_warps) to zero (release), reads results back over the same window (acquire). `tb_soc_gpu` now runs a real vector-add: CPU writes A/B → kernel computes C → readback verified `C = A + B` per lane. Remaining (deferred): flush/invalidate of GPU-facing data and a full unified kernel memory (kernel image is still flashed into IMEM at synth time, not pushed over pbus).
- **G synth+measure: DONE** — `make synth` + `impl.tcl` green on Vivado 2026.1. **Synth (re-measured 2026-08-20): 7,669 LUTs (12.1%), 1,584 LUT-as-memory, 2,410 FF, 64.5 BRAM tiles (47.8%), 15 DSP, WNS +3.348ns. Impl (place+route): 7,516 LUTs (11.9%), 1,580 LUT-as-memory, 2,410 FF, 64.5 BRAM, 15 DSP; WNS +0.685ns @50MHz → Fmax ≈ 51.8 MHz** (critical path `u_cpu/u_mem/mem_wb_rdata` → `u_cpu/u_exec/ex_mem_alu_result`, 14 logic levels incl. 3 DSP48E1). Bitstream off (no board). Numbers drift slightly vs the earlier session because the pipeline fix (decode hold>stall) added a mux level; both runs green.
- **H FreeRTOS: DONE (scheduler up)** — FreeRTOS V10.5.1 (vendored under `firmware/third_party/FreeRTOS`, M-mode CLINT port) boots on the SoC: `tb_soc_rtos` PASS — task A (prio 2) ticks 1-4 at exactly 25.05M cycles apart (500ms @ 50MHz), task B (prio 1) prints at 60.09M (1200ms), preemption correct. The bring-up exposed a real core bug (`load;load;branch` mis-evaluated because `decode_stage` gave the hazard `stall` priority over the pipeline-freeze `hold`, erasing a frozen load's `mem_read`) — fixed, full regression green; see `docs/rtos_bringup.md` + `tests/rtos/load_branch_bug.md`. **Queues DONE:** `tb_soc_rtos_q` PASS — producer/consumer tasks, 1000 items through a depth-8 `xQueue` in strict order (2.46M cycles); app `firmware/apps/rtos_q.c`, firmware Makefile links FreeRTOS for both `rtos` and `rtos_q`. **ISR-semaphore DONE:** machine-software-interrupt path added (CLINT msip reg @0x0200_0000 in `timer.v` → `csr_file.mip_msip` → `trap_unit` takes mcause 0x80000003, MSI priority over MTI); `tb_soc_rtos_sem` PASS — semaphore delivered from ISR 3/3 at exact 200ms spacing (app `firmware/apps/rtos_sem.c`). Gotcha: the port only enables MTIE/MEIE, so firmware must `csrs mie, 0x8`. **Scheduler metrics DONE:** `tb_soc_rtos_metrics` PASS (app `firmware/apps/rtos_metrics.c`, `configUSE_TICK_HOOK=1` kernel-wide with a weak default hook in `rtos/omni_libc.c`) — ISR entry latency **92 cycles avg (1.84µs)**, 2-switch `taskYIELD` round trip **492 cycles avg (9.84µs)**, tick deltas **49,806–50,192** (ideal 50,000) over 1200+ ticks. **Second core bug found by rtos_q with the tick hook enabled:** an interrupt taken while (a) a branch redirect was resolving or (b) a load sat in MEM with its forwarded value feeding the EX instruction captured an inconsistent (mepc, register-file) pair — the trap returned to a stale redirect target or re-executed an instruction against a stale register (queue ORDER FAIL). Fixed in `exec_stage.v`/`trap_unit.v`: `redirect_pending` is now the drain window only (the same-edge `redirect_valid` would mux the PREVIOUS redirect's target into mepc), and `int_window` requires `~ex_mem_mem_read` (no interrupt while a MEM-stage load is in flight). With the hook on, `tb_soc_rtos_q` still PASSes at 2.46M cycles; full regression green. **Interactive shell DONE (R3/R4):** `tb_soc_shell` + `firmware/apps/shell.c` — you can type into a FreeRTOS shell in Verilator: full-duplex UART fed from stdin at real 115200 framing (434 clk/bit, no zero-delay cheat), line-at-a-time pacing so the 16-byte RX FIFO never overruns, `quit` marker ends the sim (no fixed cycle cap, 10G safety kill). Commands: help / uptime / ps (`uxTaskGetSystemState`, `configUSE_TRACE_FACILITY=1`) / ticks (live tick-jitter from the strong tick hook in shell.c) / gpu (Phase-F host-window vector-add, sum=110 PASS over MMIO) / quit, plus a U-Boot-style debug-monitor set (peek/poke/mdump/gpio/mtime/heap/reboot, hex args; `scripts/console.sh` gives a raw-mode serial-terminal feel). Gate: repeatable scripted session `tests/test_shell.sh` < `tests/shell_session.txt` — asserts banner, command responses, backspace editing, GPU checksum, QUIT. Full roadmap: `docs/plan_rtos_to_terminal.md`.
- **I UART RX: DONE** — `uart.v` is full duplex: 2FF-synced deserializer (mid-start-bit glitch rejection, LSB-first, stop not enforced) feeding a **16-byte RX FIFO**, registers at +0x04 STATUS (bit1 rx-ready, bit2 sticky overrun) and +0x08 RXDATA (read pops + clears overrun). Occupancy is an explicit 5-bit counter — 4-bit pointers can't distinguish full from empty at depth 16. Firmware: `uart_getchar()`/`uart_status()` in `drivers/uart.c`; echo app `apps/uart_echo.c`. TBs: `tb_uart_rx` (unit, drives the `uart` module directly via a new DUT mapping) and `tb_soc_uart_rx` (echo round trip through soc_top: single byte, back-to-back pair, 0x00–0xFF sweep exact, 64-byte burst → in-order prefix + recovery, glitch immunity).  **Packaging DONE (R5):** README carries the typed-session GIF (`docs/shell_demo.gif`, regenerated by `scripts/make_shell_demo.py` from a real tb_soc_shell capture) + replayable `docs/shell_session.cast`. **The shell flushed out a real UART RTL bug:** `uart.v`'s pbus decode (`addr[31:20]==0x400 && ~addr[12]`) aliased the GPU page (0x40002000, bit 13) — `gpu_launch`'s WARP_PC0 write (+0x00 offset) matched the TX register offset and reloaded the TX shift register mid-frame, injecting a NUL byte into the output (invisible in tb_soc_gpu where TX is idle during GPU MMIO). Fixed to an exact 4KB decode (`addr[31:12]==20'h40000`); regression + `make synth` green (7,746 LUTs, WNS +3.348ns). Regression list now includes: tb_soc_shell (via tests/test_shell.sh), tb_soc_uart_rx, tb_uart_rx.
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
make impl                    # Vivado place+route (needs make synth first; reports Fmax)
make clean
```

Demo path (firmware → testbench → gate → live console): `./scripts/setup_demo.sh`, or step by step `make firmware APP=shell` → `./hardware/sim/run_sim.sh soc/tb_soc_shell` → `./tests/test_shell.sh` → `./scripts/console.sh`.

Firmware apps live in `firmware/apps/`: `make APP=benchmark_cpu`, `APP=benchmark_gpu`, `APP=timer`.

Direct testbench invocation (more control than `make sim`):
```bash
./hardware/sim/run_sim.sh cpu/tb_alu --wave    # --wave opens GTKWave on the VCD
```

## Simulation flow

`hardware/sim/run_sim.sh` is the canonical build/run script; new RTL/TBs must follow these conventions or it won't find them:

- TB C++ lives at `hardware/sim/<path>/tb_<name>.cpp`. DUT auto-derived as `<name>` → searched in `hardware/rtl/**` as `<name>.v`/`.sv`. **Module filename must match module name and DUT name** (`--top-module` is derived).
- **Special DUT mappings baked into the script** (TB name overrides auto-derive):
  `tb_soc_*`→`soc_top`, `tb_cache`→`l1_dcache`, `tb_icache`→`l1_icache`, `tb_compliance`→`cpu_top`, `tb_dual_*`/`tb_litmus_*`→`dual_core_top`, `tb_gpu_top_*`→`gpu_top`, `tb_uart_rx`→`uart`.
  Caveat: `tb_lrsc`, `tb_2store`, `tb_cpu2` instantiate `cpu_top`/`cpu2_top` but have **no mapping** — they're built from existing `obj_dir_*` trees, not a fresh `make sim`.
- Verilator flags: `--cc --exe --trace --Wall -Wno-UNUSED -Wno-UNDRIVEN`; build dir `hardware/sim/obj_dir_<tb>`, VCD → `hardware/sim/waves/<tb>.vcd`. All RTL dirs are `-I` include paths.
- `run_sim.sh` auto-builds staging hex for `tb_dual_spin` (spinlock.S), `tb_soc_rtos` (`firmware APP=rtos` → rtos.hex), and `tb_soc_gpu` (APP=benchmark_gpu + `gpu_asm.py`); the DUT memory loads these as `program.hex`/`gpu_demo.hex` via `$readmemh`. Note: firmware `make clean` deletes **all** `firmware/**/*.hex`, so re-run the affected TB (or `make synth`) afterwards.

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
- Flags: `-march=rv32im_zicsr_zifencei -mabi=ilp32 -Os -ffreestanding -nostdlib -fno-pie`; linker script `firmware/boot/linker.ld`; entry via `boot/startup.S` + `trap_handler.S` + `boot.c`.
- **PIE is fatal on this target (verified 2026-10-06).** The Linux-targeting `riscv64-linux-gnu-gcc` (CachyOS 16.2.1) defaults to `-fPIE`, which turns `la sp, _stack_top` into a GOT-relative load (`auipc sp,0x10; lw sp,204(sp)`) plus gp/GOT assumptions `startup.S` never sets up. The image links and boots, prints the banner, then corrupts itself into an **endless reboot loop** — the TX transcript repeats `[SHELL] readOmni-RISC APU v1.0` forever (this is what "the demo hangs with no output" looks like; `test_shell.sh` hides it by logging the transcript to a temp file). Firmware must build non-PIC: `-fno-pie` in `CFLAGS` (and `FREERTOS_ASM_FLAGS`) + `-no-pie` on **both** link lines, all now in `firmware/Makefile`. Signature to check first: `readelf -h firmware/<app>.elf | grep Type` → `EXEC` good, `DYN` = PIE = broken.
  - Do **not** put `-fno-pie` on the link line: with binutils 2.47 `ld` dies with `collect2: fatal error: ld terminated with signal 11 [Segmentation fault]`. `-no-pie` is the link flag that works.
- **binutils 2.47 (CachyOS) rejects the pre-PHDRS linker scripts:** without an explicit `PHDRS`, ld synthesises a `PT_PHDR` segment for the ELF headers and errors `PHDR segment not covered by LOAD segment` (the older toolchain warned instead) — a PIE link triggers it, and `-no-pie` alone avoids it. Fixed anyway by declaring `PHDRS { image PT_LOAD; }` (`linker.ld`) / `{ text PT_LOAD; data PT_LOAD; }` (`linker_rtos.ld`) and tagging the sections; the flat image is unchanged (entry `0x0`, `.bin` offset 0 = first instruction of `_start`).
- `firmware/Makefile` needs `.DEFAULT_GOAL := all`: `-include $(DEPS)` injects a rule from `boot/startup.d` as the first target, so a goal-less `make -C firmware` (exactly what the root `make firmware` runs) built `boot/startup.o`, printed "is up to date" and **exited 0 without producing a hex**.
- Verified with GCC 16.2.1 + binutils 2.47: `./scripts/setup_demo.sh --no-run` → firmware + testbench build + `tests/test_shell.sh` `ALL PASS` (17 checks). **Untested suspect:** `tests/run_compliance.sh` links with `-nostdlib -nostartfiles` and no `-no-pie` (the riscv-arch-test clone is absent/gitignored here) — if `make compliance` misbehaves under a new toolchain, check its PIE status first.
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

- `hardware/rtl/cpu/core/` — one module per stage (`fetch/decode/exec/mem/wb_stage`), datapath (`alu`, `decoder`, `imm_gen`, `regfile`, `forwarding_net`, `branch_unit`, `multiplier`, `divider`, `lsu`, `pc_gen`), control (`pipeline_ctrl`, `hazard_unit`, `csr_file`, `trap_unit`); `cpu_top.v` wires them. Stub files carry a spec header — implement to that spec. Multicore: `cpu2_top.v` (2 cores + pbus arbiter), `dual_core_top.v` (shared data_bram + snoop bus, see `docs/history/roadmap.md` Phase D).
- `hardware/rtl/cpu/cache/` — `l1_dcache.v`, `l1_icache.v`, `l1_dcache_msi.v`. Gated by `USE_CACHES` param on `cpu_top` (0=direct BRAMs for tb_cpu_top/compliance, 1=cached SoC). `mem_stage` has `SHARED_MEM` param (1 = dcache backing memory external, dual-core).
- `hardware/rtl/gpu/` — `gpu_top.v` (pbus slave) + `gpu_cmd_proc.v` over `core/` (`gpu_fetch`, `gpu_decode`, `warp_scheduler`, `exec_lane`, `gpu_alu`, `gpu_lsu`, `gpu_regfile`, `gpu_scratchpad`).
- `hardware/rtl/soc/` — live UART/timer(CLINT)/GPIO + `soc_top.v`. (The old `hardware/rtl/peripherals/` and the unused AXI bus modules in `hardware/rtl/bus/` were deleted as dead code; only `bus/pbus_arbiter.v` remains, used by `dual_core_top`.)
- `tests/omni-risc/` — compliance reference interpreter (`ref_sim.cpp`) + link/macros. `tests/riscv-arch-test/` and `tests/compliance_work/` are cloned/generated (gitignored).
- `docs/memory_map.md` — SoC address map; firmware drivers and the pbus decode must agree with it. `docs/cpu_design.md` — pipeline spec + 10-step bring-up order (historical; phases done).

## Demo setup

`scripts/console.sh` = live raw-mode serial terminal into the simulated APU (`tb_soc_shell`); `tests/test_shell.sh` = its scripted, non-interactive gate; `scripts/setup_demo.sh` = fresh-clone bootstrap in one command:

```bash
./scripts/setup_demo.sh          # dep check -> build firmware + tb -> gate -> launch console.sh
./scripts/setup_demo.sh --check  # dependency check only (exit 1 if a package is missing)
./scripts/setup_demo.sh --no-run # build + gate only (CI / non-TTY)
```

- **`run_sim.sh` builds the firmware for `tb_soc_*` with stdout/stderr silenced** and the root `Makefile` has no `test` target — so a missing toolchain or a stale image shows up as a baffling mid-session failure. `setup_demo.sh` builds `shell.hex` first with the output visible, then stages it (plus `gpu_demo.hex`) into `obj_dir_tb_soc_shell`.
- Everything the demo needs *besides* packages is in git (FreeRTOS is vendored, not a submodule). All `*.hex` are gitignored → **a fresh clone has none**, so the demo cannot run before a firmware build.
- **Verified end-to-end 2026-10-06** (CachyOS `riscv64-linux-gnu` gcc 16.2.1 / binutils 2.47): `setup_demo.sh --no-run` → firmware + tb build → `test_shell: ALL PASS` (17 checks, incl. `gpu sum=110 irq=1 PASS`). The demo also needs the two toolchain fixes recorded under *Firmware toolchain* below (`-fno-pie`/`-no-pie`, `PHDRS`) or it reboot-loops instead.

## Firmware track (F1–F8)

`docs/plan_firmware_track.md` is the plan; **all eight phases are implemented and green**. Each phase is an interface + a gate app + an implementation, and the gate runs the app on the SoC through `hardware/sim/soc/tb_soc_console.cpp` (the generic console TB, not `tb_soc_shell`).

```bash
./tests/test_libc.sh      # F1 memory/string (130 checks)     ./tests/test_convert.sh  # F1 conversions (260)
./tests/test_printf.sh    # F2 vs glibc (93 vectors + 2 stream diffs)
./tests/test_uart.sh      # F3 UART RX: burst (loss accounted) + paced line/timeout
./tests/test_framing.sh   # F4 CRC framing vs a byte-level receiver simulator
./tests/test_alloc.sh     # F5 allocator (12 checks, 2000-op soak, heap_check after each op)
./tests/test_fault.sh     # F6 fault kinds (illegal / misaligned / ecall / ebreak)
./tests/test_timers.sh    # F7 time + software timers (13 checks)
./tests/test_gpu_loader.sh # F8 runtime kernel load: pushed vs not-pushed
```

- **Harness**: `tests/run_app.sh <app>` builds `APP`, stages the hex into the console TB's `obj_dir`, runs it, and returns its status. Env: `APP_DEFS` (per-run defines), `TB_START_MARK` (hold input until the app says ready), `TB_PACE=burst|line`, `TB_QUIET` (cycles of TX silence that mean "finished" — **raise it for a long CPU-only test**), `TB_MAX`, `TB_GPU_IMAGE=none` (don't stage a GPU image). Assertions come from `tests/check.sh` (`check_init/has/count/absent/pass`), whose greps are binary-safe; `check_pass` dumps the app's FAIL lines.
- **Gate apps carry weak stubs** for every symbol they need (strong definitions in `firmware/lib`, `firmware/drivers`, `firmware/net` override them), so a gate runs, prints per-case diagnostics, and fails informatively before the code exists.
- **Gate apps build with `-fno-builtin`** (`apps/%_test.o` in the Makefile) — otherwise gcc folds their scaffolding into `memcpy`/`strlen` or folds `snprintf(buf, n, "literal")` into a copy, and the gate tests the compiler instead of the code.
- Generated vector tables (`firmware/apps/*_vectors.h`, `*_expected.h`, `gpu_kernel_words.h`) and `tests/generated/` are **gitignored**; the gates generate them from a seed (`--seed N`) so a failure is reproducible.
- **Traps this track keeps re-teaching**: no libgcc in a `-nostdlib` link, so a 64-bit divide or multiply (`__udivdi3`, `__muldi3`) is an undefined symbol — saturate in 32-bit and accumulate in the ISR; a test that is CPU-only and prints nothing looks identical to a hung one to the harness, so raise `TB_QUIET` and emit progress; after any RTL change the console TB must be rebuilt (`rm hardware/sim/obj_dir_tb_soc_console/Vsoc_top`).
- **F8 added a load port** to the GPU: `0x4000_2024` IMEM_ADDR (RW word index) and `0x4000_2028` IMEM_DATA (store installs that 16-bit word and advances the pointer). `gpu_fetch`'s array is write-only and reset-free to stay RAM-inferable. The write decode in `gpu_cmd_proc` is now 6-bit (it was 5, which aliased a write to `+0x20` onto `warp_pc[0]`). Completion is waited on as an interrupt: GPU done → CLINT `msip` → `mcause 0x80000003` → the weak `omni_msi_isr()` hook in `firmware/boot/fault.c`, overridden by `drivers/gpu_loader.c`.
- **F6 finding worth remembering**: this core has **no MMU/PMP**, so load/store access faults cannot occur, and it does **not** raise misaligned-access exceptions (the LSU just performs the access). `firmware/boot/fault.c` splits the trap path — interrupts to their old handlers, exceptions to a decoded dump with the 28-register frame, and `ecall` from M-mode as a deliberate `[TEST] DONE` exit.
- **F8 is Verilator-verified only** — this host has no Vivado, so its RAM-inference claim is argued from the port structure, not measured.

## Unified memory (M1 done, M2 done, M3 not)

The GPU used to be a pbus *slave* only: kernels could reach the outside world only through the CPU-manned host window into warp0's scratchpad. "Unified memory" means kernels address the same `data_bram` the CPU's ordinary loads and stores use.

- **M1 (done, commit 35944dd)**: `data_bram` moved out of `cpu_top` to `soc_top` behind `hardware/rtl/bus/mem_arbiter.v` — a two-master req/ack arbiter (CPU priority, `rtag` tracking whose registered read is in flight; a GPU write that loses arbitration is not taken and is retried). `mem_stage` gained the **cacheless** `SHARED_MEM` arm, which had never been exercised: the `mem_if_*` assigns only existed inside a `USE_CACHES` generate block, so `SHARED_MEM=1` with the SoC's `USE_CACHES=0` would have returned 0 for every load. `cpu_top`/`mem_stage` also gained **`mem_if_wen`**: the interface had no write-enable port because its only previous user was the dcache (whose write-through is full-word), and fabricating `4'hF` turned every `sb`/`sh` into a word store. It deliberately keeps the internal bram instantiated (inert) so `tb_cpu_top`/compliance can still peek `u_mem->u_dbram->mem`.
- **M2 (done, commit 5a64e53)**: `LDG`/`STG` (GPU opcode 7) with a 32-bit address in `rs1`, through `gpu_lsu.g_addr` → `exec_lane` → `gpu_top`'s master port → `mem_arbiter`. The accesses are **warp-uniform** (lane 0's address, one transaction, loaded word broadcast), which keeps the master single-outstanding. `GPU_MEM_BASE` (GPU `+0x202C`) points the GPU at a CPU buffer so kernels use small offsets. Handshake: a transaction starts when the instruction is *presented*, and the instruction is **accepted only when its data has arrived**, so the lane writeback lands on the acceptance cycle; `issue_ready` carries that gate and `exec_lane`'s `sel` is gated by it. Two bugs found here, both by instrumentation: the request address/data were taken combinationally off the issue stage (the transaction outlives that cycle, so the store went to another warp's address — `dbg_last` showed base+0 instead of base+8), now latched at start; and a stale `g_done` could let the next global instruction be accepted with no transaction. GPU debug registers (read-only, they stay): `+0x2030` dbg_starts, `+0x2034` dbg_state, `+0x2038` dbg_last.
- **M3 (NOT done)**: `firmware/apps/mlp_mem_test.c` + `tests/test_mlp_mem.sh` port the 2-layer MLP onto shared memory. **The gate currently fails**: all four samples are wrong, and it costs **15125 cycles against the MMIO window's 6278** — 2.4x *worse*. Reasons to fix before claiming a win: every kernel access is a bus transaction that arbitrates against a CPU polling memory in its wait loop (the arbiter gives the CPU priority on purpose), and warp-uniform accesses serialize the samples that four SIMT lanes used to do in parallel. `tests/test_gpu_mem.sh` (a simple add kernel) passes, so the mechanism works; the MLP port is what does not.
- **Still deferred**: flush/invalidate of GPU-facing data is only meaningful with `USE_CACHES=1` (the SoC runs cacheless), and there is still no silicon: everything above is Verilator.

## Requirements

| Package | Needed for | Arch/CachyOS package |
|---|---|---|
| Verilator ≥ 5.0 (5.052 verified) | every TB | `verilator` |
| `riscv64-linux-gnu-{gcc,as,objcopy}` (rv32 multilib) | firmware + `tb_dual_spin`'s `spinlock.hex` | `riscv64-linux-gnu-gcc` |
| Python 3 | `elf_to_hex.py`, `gpu_asm.py`, `test_gpu_kernels.py` | `python` |
| numpy, pyelftools (`requirements.txt`) | `tests/test_gpu_kernels.py`; `scripts/elf_to_hex.py` (the `firmware/elf_to_hex.py` used by the build is self-contained) | `python-numpy`, `python-pyelftools` |
| GTKWave / asciinema / Pillow | `--wave` / replay `docs/shell_session.cast` / regenerate `docs/shell_demo.gif` | `gtkwave`, `asciinema`, `python-pillow` |
| Vivado 2026.1 at `/tools/2026.1/Vivado` | `make synth` / `make impl` only | — |

Toolchain prefix is overridable: `make PREFIX=...` or `TOOLCHAIN_PREFIX=... ./scripts/setup_demo.sh`.

**Known doc drift:** README's "Fits on the board" line (7,639 LUTs, WNS +1.253 ns → 53.4 MHz) does not match the post-R5 impl numbers above (7,516 LUTs, WNS +0.685 ns → 51.8 MHz); re-run `make impl` before quoting either.
