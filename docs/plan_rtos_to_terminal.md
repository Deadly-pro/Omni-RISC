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

### R1c. Scheduler metric numbers (the quotable ones)  **(DONE)**
Measure with `mtime` reads (50k cycles/tick resolution) and dump over UART:
- **ISR entry latency: 92 cycles avg (1.84 µs)** — 20 msip-raise samples.
- **2-switch taskYIELD round trip: 492 cycles avg (9.84 µs)** — 200 trips.
- **Tick jitter: 49,806–50,192** (ideal 50,000), max deviation 194 cycles over 1200+ ticks.
- Firmware: `firmware/apps/rtos_metrics.c`, TB: `tb_soc_rtos_metrics`.
- Write results into `docs/rtos_bringup.md` + AGENTS.md Phase H ("DONE" numbers).
- **Why:** these three numbers are what you quote in interviews; "I measured
  1.8 µs context switch on my own core" beats "it runs FreeRTOS".

## R2 — UART RX in RTL  (**DONE**, Aug 2026)

Full duplex now: deserializer + 16-byte FIFO in `uart.v`, registers at
+0x04 STATUS (bit1 rx-ready, bit2 sticky overrun) and +0x08 RXDATA
(read-pops). Driver: `uart_getchar()` / `uart_status()` in
`firmware/drivers/uart.c`. Two testbenches:

- `tb_uart_rx` (unit, drives the `uart` module directly): idle status,
  glitch rejection, single byte, empty-read safety, back-to-back frames,
  16-byte fill in order, overrun set/drop/clear — all deterministic.
- `tb_soc_uart_rx` (integration, echo app `apps/uart_echo.c`): single
  byte, back-to-back pair, full 0x00-0xFF sweep exact, 64-byte burst
  degrades to an in-order prefix then recovers.

Design note: FIFO occupancy is an explicit 5-bit counter — 4-bit pointers
cannot distinguish full from empty at depth 16 (caught by tb_uart_rx).
pbus reads are single-cycle strobes for peripheral loads, so pop-on-read
is race-free. Vivado rules held: no reset on the inferred RAM array.

## R3 — Interactive simulation harness  (**DONE**, Aug 2026)

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
- **GATE PASSED** — `tests/test_shell.sh` pipes `tests/shell_session.txt`
  (help/uptime/ps/ticks/gpu/foo/backspace-edit/quit) into the sim at real
  framing and asserts the full transcript; all checks green.
- Two harness lessons baked into `tb_soc_shell.cpp`: stdin is pulled ONE
  LINE at a time (the 16-byte RX FIFO cannot absorb a pasted script while
  the console task sleeps up to 2ms), and each line waits for its response
  to drain (>=4ms since newline AND TX quiet >=2ms — global TX silence
  alone releases while the answer is still pending). Frames carry a 2-bit-
  time idle gap; typed bytes echo to stderr, decoded TX streams live to
  stdout.

## R4 — The shell firmware  (**DONE**, Aug 2026)

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
- **GATE PASSED** — same script as R3. App: `firmware/apps/shell.c`
  (`APP=shell`, linked with FreeRTOS). Console task polls `uart_getchar()`,
  echoes, backspace (`0x7F`/`0x08` -> `" "`), dispatch table.
  `ps` uses `uxTaskGetSystemState` (`configUSE_TRACE_FACILITY=1`; no
  run-time-stats timer needed); `ticks` reads the strong tick-hook's
  min/max/count + last-16 ring; `gpu` reuses the Phase-F host window
  (vector-add sum=110 PASS over MMIO); `quit` prints `[SHELL] QUIT`, which
  the TB decodes to end the sim. `strcmp` added to `rtos/omni_libc.c`.

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
