# Plan — RTOS completion through to an interactive terminal (Aug 2026)

End state: you type into a FreeRTOS-driven shell, in Verilator, on your own
RV32IM core, and a shell command launches a GPU kernel over MMIO. That single
demo covers CPU pipeline, RTOS porting, SoC integration, and the SIMT GPU at
once — it is the strongest 90-second demo the project can produce.

Order matters: R1 finishes the RTOS story (it's already half-promised in
AGENTS.md), R2–R3 build the RX path, R4 makes it interactive, R5 packages it
for the resume/interview. Each phase ends with a green gate before moving on.

---

## R1 — Finish the RTOS: queues, ISR semaphore, metrics  (~2-3 sessions)

The remaining deliverables already named in AGENTS.md Phase H.

### R1a. Queues between tasks  (DONE — first pass)
- Firmware app `firmware/apps/rtos_q.c`: producer task (prio 1) sends
  1..1000 into a depth-8 queue; consumer task (prio 2) blocks on
  `xQueueReceive` and verifies strict ordering.
- TB: `hardware/sim/soc/tb_soc_rtos_q.cpp` (uses the standard
  `tb_soc_*` → `soc_top` mapping; run_sim.sh stages `rtos_q.hex`; firmware
  Makefile now links FreeRTOS for both `rtos` and `rtos_q`).
- **Gate: PASSED** — `[Q] QUEUE DONE 1000`, all 4 progress reports in order,
  2,459,685 cycles total, zero ORDER FAIL/ASSERT lines.

### R1b. Semaphore given from an ISR (real RTL work)  (DONE — first pass after one fix)
- Implemented the full machine-software-interrupt path:
  - `timer.v`: CLINT msip register @ +0x0000 (bit0, read/write) → `msip` output.
  - `csr_file.v`: `msip` input ORs into mip.MSIP (CSR-write path kept).
  - `trap_unit.v`: takeable MSI (`mstatus.MIE & mie.MSIE & mip.MSIP`),
    cause 0x80000003, MSI priority over MTI per spec order.
  - Wired through `exec_stage` → `cpu_top` → `soc_top`; `dual_core_top` and
    `cpu2_top` tie msip off.
- Firmware `firmware/apps/rtos_sem.c`: trigger task raises CLINT msip 3x;
  `freertos_risc_v_application_interrupt_handler` clears the source,
  `xSemaphoreGiveFromISR` + `portYIELD_FROM_ISR`; waiter task counts to 3.
- **Gotcha found:** the FreeRTOS port only enables MTIE/MEIE — firmware must
  `csrs mie, 0x8` or msip pends forever with no trap.
- TB: `hardware/sim/soc/tb_soc_rtos_sem.cpp` (+ `TB_DEBUG=1` env prints all
  decoded UART lines for bring-up).
- **Gate: PASSED** — `[SEM] GOT 1..3` at exactly 200ms spacing, DONE at
  30.7M cycles; regression green (tb_cpu_top 83/83, compliance 53/54,
  tb_soc_rtos, tb_soc_rtos_q, tb_dual_spin).

### R1c. Scheduler metric numbers (the quotable ones)
Measure with `mtime` reads (50k cycles/tick resolution) and dump over UART:
- Context-switch cost: cycles around a forced `taskYIELD()` between two
  pinned tasks, median of 100 switches.
- Tick jitter: actual cycles between consecutive ticks vs 50,000, over ≥1000 ticks.
- ISR latency: known-cycle interrupt raise → first instruction of handler.
- Write results into `docs/rtos_bringup.md` + AGENTS.md Phase H ("DONE" numbers).
- **Why:** these three numbers are what you quote in interviews; "I measured
  1.8 µs context switch on my own core" beats "it runs FreeRTOS".

## R2 — UART RX in RTL  (~1-2 sessions)

Today `uart.v` is TX-only (`soc_top.v:19` — `uart_rx` unused). Memory map says
so too (`docs/memory_map.md`: "TX-only 115200-8N1").

- Add RX deserializer to `uart.v`: synchronize rx, wait for start bit edge,
  sample at mid-bit using the existing baud counter (115200 @ 50 MHz ≈ 434
  clocks/bit), assemble 8 bits, push into a small FIFO (or single holding reg
  for v1). Status bit "rx ready" + read-clears data register, appended to the
  existing UART address space per `memory_map.md` conventions.
- Vivado gotchas apply: registered read if the holding buffer becomes RAM-like,
  no reset on inferred RAM, module output ports must not be `reg`-driven by
  assigns (see AGENTS.md physical-design notes).
- Firmware: extend `firmware/drivers/uart.c` with blocking `uart_getchar()`
  (poll rx-ready).
- TB: new `hardware/sim/soc/tb_uart_rx.cpp` driving `uart_rx` bit-banged at
  434-clock periods; test framing across byte boundaries + back-to-back bytes.
- **Gate:** tb_uart_rx PASS; `make synth` still clean.

## R3 — Interactive simulation harness  (~1 session)

Verilator TBs here are batch (run to MAX_CYCLES, print UART to stdout). Make
one that talks back:

- New `hardware/sim/soc/tb_soc_shell.cpp` (reuses `soc_top` mapping):
  - Each cycle, drive `uart_rx` from stdin: feed bytes at real baud timing
    (434 clk/bit — don't cheat to zero, the deserializer must see valid
    framing; that's part of the demo's honesty).
  - No fixed cycle cap: run until stdin closes OR the shell's `quit` command;
    safety kill at ~10G cycles.
  - Line-buffered terminal echo so typing feels live (VCD off by default,
    `--wave` flag like other TBs).
- Wire staging into `run_sim.sh` following the `tb_soc_rtos` block
  (`firmware APP=shell` → program.hex).
- **Gate:** you can type `help<Enter>` and see the echo + response.

## R4 — The shell firmware  (~1-2 sessions)

New app `firmware/apps/shell/`, FreeRTOS-based:

- Console task: blocks on `uart_getchar()`, line-editing (backspace min),
  dispatch table. Keep one task polling first; upgrade to queue-fed-from-IRQ
  only after R1b exists.
- Command set (v1, all verifiable):
  - `help` — command list
  - `uptime` — tick count → seconds
  - `ps` — task list via `vTaskList` (enable `configUSE_TRACE_FACILITY` +
    stats in FreeRTOSConfig.h)
  - `ticks` — last-N tick jitter histogram (feeds straight from R1c)
  - `gpu` — reuses the Phase-F HOST_WIN/HOST_DATA path: write A/B vectors,
    launch vector-add over MMIO, poll STATUS, print checksum → ties the GPU
    into the demo
  - `quit` — exits cleanly so the sim terminates (sets a tohost-style flag)
- **Constraint reminder:** split I/D BRAMs mean no self-modifying code; keep
  everything compiled-in. Hexes are gitignored — shell.hex regenerates via
  `make firmware APP=shell`.
- **Gate:** scripted session test — a `.txt` of piped commands produces
  expected output diff (make it a repeatable `tests/test_shell.sh`).

## R5 — Package it  (~0.5 session)

- README: short GIF/asciinema of a typed session (`ps`, `ticks`, `gpu`).
- AGENTS.md: mark Phase H fully done with metric numbers; add shell TBs to
  the regression list.
- One-paragraph resume bullet: RV32IM APU, FreeRTOS V10.5.1 port with
  measured context-switch/jitter numbers, interactive UART shell in
  simulation, GPU kernel launched from shell command.

---

## Sequencing & risk

| Phase | Depends on | Risk |
|---|---|---|
| R1a | nothing | low — pure firmware/TB |
| R1b | nothing | **medium — touches trap_unit/csr_file; regression gate is mandatory** |
| R1c | R1a/R1b optional | low |
| R2 | nothing | medium — new serial RTL; bit-timing bugs show up only under R3 |
| R3 | R2 | low |
| R4 | R2 (+R3 to demo) | low |
| R5 | R4 | none |

Do R1b early and alone: it's the only phase that can break compliance, and if
it does you want the blast radius isolated, not stacked under R2 changes.
R3's honest-baud requirement is deliberate — a fake instant-RX would make the
RTL untestable and the demo dishonest.
