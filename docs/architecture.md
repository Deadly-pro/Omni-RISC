# Omni-RISC APU — Architecture

What the machine **is**, as built (Aug 2026). For how it got here see
`AGENTS.md` (status) and `docs/history/` (planning docs).

## The machine in one paragraph

A single-FPGA SoC on Xilinx Artix-7 (XC7A100T): one RV32IM CPU core (5-stage
in-order pipeline, M-extension, M-mode CSRs/traps) and a SIMT GPU (4 warps ×
4 lanes, 16-bit instruction word) sharing one address space over a simple
peripheral bus (pbus). Peripherals: full-duplex UART (115200-8N1, 16-byte RX
FIFO), CLINT timer (mtime/mtimecmp + msip software interrupt), GPIO. The CPU
runs FreeRTOS V10.5.1 (vendored, M-mode CLINT port) and an interactive UART
shell whose `gpu` command launches a vector-add kernel on the GPU through a
coherent shared-memory window. A separate 2-core configuration
(`dual_core_top`) demonstrates fine-grained snooping-MSI cache coherence.

## Top-level structure (`hardware/rtl/soc/soc_top.v`)

```
                 +--------------------------------------------+
   uart_rx ----> |                  soc_top                   | ---> uart_tx
                 |                                            |
                 |  +--------+   pbus (MMIO >= 0x4000_0000)   |
                 |  | cpu_top|----+-----+------+-----+        |
                 |  | RV32IM |    |     |      |     |        |
                 |  +--------+  UART  timer   GPIO   |        |
                 |       |                  (CLINT)  |        |
                 |  IMEM 64KB              +-----v-----+  |
                 |  DMEM 256KB             |  gpu_top  |  |
                 |  (BRAM in cpu_top)      |  SIMT 4x4 |  |
                 |                         +-----------+  |
                 +--------------------------------------------+
```

- **Memory**: IMEM `0x0000_0000`–`0x0000_FFFF` (64KB, `instr_bram`, holds the
  firmware image loaded via `$readmemh` at time zero) and DMEM
  `0x0001_0000`–`0x0003_FFFF` (256KB, `data_bram`). Full map:
  `docs/memory_map.md`.
- **pbus**: a single-cycle MMIO strobe bus. Any address `>= 0x4000_0000` is
  decoded by the slaves (UART `0x4000_0000`, GPIO `0x4000_1000`, GPU
  `0x4000_2000`); each slave drives `pbus_rdata` only when selected and the
  top level ORs them. Everything below is the internal data BRAM. There is
  no AXI anywhere in the design.
- **Interrupts**: the CLINT asserts `mtip` when `mtime >= mtimecmp` and the
  `msip` register (software-raised, written at `0x0200_0000`) ORs into
  `mip.MSIP`. The trap unit takes MTI and MSI in spec priority order
  (MSI first), vectoring through `mtvec` to the FreeRTOS trap handler.

## CPU core (`hardware/rtl/cpu/core/`)

RV32IM + Zicsr, machine mode only, single hart in `soc_top`.

- **Pipeline**: IF → ID → EX → MEM → WB, one module per stage, wired in
  `cpu_top.v`. Full forwarding (EX→EX, MEM→EX, MEM→MEM), hazard detection
  with load-use stalls, static not-taken branches with EX-stage redirect.
- **Memory timing**: `data_bram` has a registered read (that is what makes
  it BRAM instead of 32k LUTs), so every load spends an extra cycle in MEM
  (`mem_hold` in `mem_stage.v`). Level-req handshakes (`dc_mem_ack`) cover
  the hold.
- **M-extension**: iterative `multiplier.v` (3 DSP48E1) and `divider.v`
  (shift/subtract), all eight mul/div/rem variants including the signed
  edge cases.
- **CSRs/traps**: M-mode only — mstatus, mtvec, mepc, mcause, mie/mip,
  mcycle/minstret. Interrupts are taken only in a safe window: not while a
  branch redirect is draining and not while a MEM-stage load is in flight
  (`trap_unit.v` `int_window`) — both conditions produced real bugs before
  they were enforced (see `AGENTS.md` Phase H).
- **Caches** (`hardware/rtl/cpu/cache/`, gated by the `USE_CACHES` parameter
  on `cpu_top`, default 0 = direct BRAM): `l1_icache` 4KB direct-mapped,
  `l1_dcache` 4KB 2-way write-through, `l1_dcache_msi` adds the snooping
  MSI state for the 2-core rig.

## 2-core coherence rig (`hardware/rtl/soc/dual_core_top.v`)

Two `cpu_top` instances sharing one `data_bram` through a snoop bus and
`bus/pbus_arbiter.v`. Each core's D-cache runs snooping **MSI,
write-through/write-invalidate** — the only fine-grained coherence in the
design, deliberately limited to 2 agents. Atomicity via LR/SC (verified by
a 1000-iteration counter spinlock with zero lost updates and the MP/SB
litmus tests). This rig is the reference model for the CPU↔GPU contract,
not a scalable general interconnect.

## GPU (`hardware/rtl/gpu/` — details in `docs/gpu_design.md`)

SIMT engine: 4 warps × 4 lanes, own fetch/decode/warp-scheduler frontend,
16-bit instruction word, per-warp regfile, 4-bank scratchpad. Attached as a
pbus slave with a command interface (`gpu_cmd_proc.v`): the CPU writes
kernel inputs through a shared-memory window, launches a warp by register
write, polls `STATUS` until the warp halts, and reads results back through
the same window. The kernel image is flashed into GPU IMEM at synthesis
time (`gpu_demo.hex`); there is no runtime code download yet.

## Coherence model (the APU claim)

- **CPU↔CPU**: fine-grained snooping MSI per cache line (2 cores only).
- **CPU↔GPU**: coarse-grained at kernel boundaries — CPU writes inputs
  (release), launches (acquire), polls to zero, reads results. No snooping
  for the GPU; MESI/write-back/directory schemes are out of scope on
  purpose.

## Software stack (`firmware/`)

- **Boot**: `boot/startup.S` sets sp and jumps to `boot.c`, which calls the
  app's `app_main()`. Two linker scripts: `linker.ld` (bare-metal) and
  `linker_rtos.ld` (FreeRTOS apps, kernel + ISR stack in DMEM).
- **Drivers** (`firmware/drivers/`): uart (TX + FIFO RX with
  `uart_getchar`), timer/CLINT, gpio, gpu (launch/window/status helpers).
- **FreeRTOS V10.5.1**: vendored under `firmware/third_party/FreeRTOS`,
  portable/GCC/RISC-V M-mode port with the CLINT no-extension variant.
  Tick from mtimecmp at 1kHz; context switch via the trap handler;
  software interrupts (msip) deliver ISR semaphores. Kernel config in
  `firmware/rtos/FreeRTOSConfig.h`, libc stubs in `firmware/rtos/omni_libc.c`.
- **Apps** (`firmware/apps/`): hello, timer, benchmarks (CPU/GPU), RTOS
  bring-up suite (rtos, rtos_q, rtos_sem, rtos_metrics), uart_echo, and
  `shell.c` — the interactive console (help/uptime/ps/ticks/gpu/quit).

## Verification (all green, Aug 2026)

| Area | Evidence |
|------|----------|
| ISA compliance | riscv-arch-test rv32im **53/54** (1 skip: `fence.i`, split I/D BRAMs) — `docs/compliance_results.md` |
| CPU unit | tb_cpu_top 83/83 + per-module TBs (alu, decoder, regfile, csr, branches, mul/div) |
| Caches | tb_cache 14/14, tb_icache 27/27 |
| Coherence | tb_dual_spin (counter→1000, no lost updates), litmus MP + SB 200/200 |
| GPU | tb_gpu_top 7/7 kernels, tb_gpu_top_kernels 66/66 |
| RTOS | tb_soc_rtos / _q / _sem / _metrics — preemption, queues, ISR semaphores, measured metrics |
| Shell | tb_soc_shell + `tests/test_shell.sh` scripted session |

## Measured numbers

- **Implementation (place+route, 50 MHz target)**: 7,516 LUTs (11.9%),
  2,410 FF, 64.5 BRAM tiles, 15 DSP, WNS +0.685 ns → Fmax ≈ 51.8 MHz;
  critical path is mem/WB → EX ALU operand mux, 14 logic levels.
- **Synthesis** (current tree): 7,746 LUTs, 1,592 LUT-as-memory, 64.5 BRAM,
  15 DSP, WNS +3.348 ns.
- **RTOS on the core**: ISR entry latency 92 cycles avg (1.84 µs);
  2-switch `taskYIELD` round trip 492 cycles (9.84 µs); tick jitter
  49,806–50,192 cycles against the ideal 50,000 (±0.4%) over 1200+ ticks.

## Repo layout

```
hardware/rtl/cpu/      core pipeline + datapath + control, caches, cpu2_top
hardware/rtl/gpu/      gpu_top, gpu_cmd_proc, core/ (fetch, decode, scheduler, lanes)
hardware/rtl/soc/      soc_top, dual_core_top, uart, timer (CLINT), gpio
hardware/rtl/memory/   instr_bram, data_bram (BRAM inference rules matter)
hardware/rtl/bus/      pbus_arbiter (dual-core only)
hardware/sim/          Verilator TBs, run_sim.sh driver (see AGENTS.md conventions)
hardware/vivado/       synth.tcl / impl.tcl, constraints, reports
firmware/              boot, drivers, apps, gpu kernels, vendored FreeRTOS
scripts/               elf_to_hex.py, gpu_asm.py, gen_arch_diagram.py, make_shell_demo.py
tests/                 compliance runner, shell gate, GPU kernel vectors, RTOS notes
docs/                  this file, memory_map, gpu_design, compliance results, history/
```
