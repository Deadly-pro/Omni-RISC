# FreeRTOS bring-up — investigation log

## Status
The FreeRTOS kernel, RISC-V port, application, linker and a Verilator UART
testbench are all built and wired in (`firmware/third_party/`, `firmware/rtos/`,
`firmware/apps/rtos.c`, `firmware/boot/linker_rtos.ld`, `hardware/sim/soc/
tb_soc_rtos.cpp`, `tests/rtos/`). The kernel **boots and reaches the
scheduler** (`xTaskCreate` succeeds for both tasks, "starting scheduler"
prints), but **the scheduler dispatches the idle task instead of the
higher-priority user task** — no "task A tick" ever prints.

## What was ruled out (the CPU is NOT the problem)
This was investigated extensively because the symptom (scheduler picking idle)
first looked like a branch bug. It is not:

- `riscv-arch-test` compliance still 53/54.
- `tb_cpu_top` 83/83 (includes load-use interlock + single-load->beq).
- L1 cache, SoC-GPU, dual-core spinlock + litmus regressions all pass.
- Hand-assembled micro-tests (`firmware/apps/mincase.c`, `loadbr.c`) covering
  every load->branch shape — single load, two loads, dist-1 load in rs1 vs
  rs2, load-ptr->load-field->bltu — **all pass on the pristine core**,
  including the exact instruction sequence FreeRTOS's
  `prvAddNewTaskToReadyList` uses for its priority compare.
- Conclusion: the core's branches, forwarding, load-use stall, and single- and
  multi-load compare paths are correct.

## The real symptom: pxCurrentTCB becomes the idle TCB
Tracing `hardware/sim/soc/tb_soc_rtos.cpp` (which reads `pxCurrentTCB`,
`uxCurrentNumberOfTasks`, and the ready lists via the public `data_bram`):
- After `xTaskCreate(A)` + `xTaskCreate(B)`, `uxCurrentNumberOfTasks`=2 and
  `pxCurrentTCB` points at task A (heap TCB ~0x11190).
- After `vTaskStartScheduler` creates the idle task, `pxCurrentTCB` points at
  a TCB (~0x11a20) whose saved context runs `prvIdleTask`; the CPU is wedged
  in the idle spin (`pc=0x7a4`).
- The ready lists are **correct**: ready[0]=idle, ready[1]=B, ready[2]=A.
- Registers in the compare path were observed holding `0xa5a5a5a5`

  (FreeRTOS's `tskSTACK_FILL_BYTE`, stacks are pre-filled with 0xa5).

The `0xa5a5a5a5` fill leaking into TCB-struct reads is the key clue: it points
at **memory corruption / mis-addressing of the heap or TCBs**, not a compare
or dispatch-logic defect.

## Leading hypothesis: two-region linker `_stack_top` aliases in data_bram
`data_bram` indexes `mem[addr[17:2]]` (18 bits). The two-region linker
(`firmware/boot/linker_rtos.ld`) sets `_stack_top = 0x40000` and the heap /
TCBs live in the DMEM window `0x10000+`. `0x40000` has bit 18 set, so
`addr[17:2]` = **0x00000** — the pre-scheduler stack, and any data near the
top of the DMEM window, aliases onto the start of the image / heap. That can
silently corrupt the TCB / ready-list structures that live in the heap as the
scheduler starts.

(This was the exact reason the bring-up first ran with the flat `linker.ld`;
the two-region layout was introduced only to fit a 64KB heap, and it makes the
shadowing worse.)

## Next steps (ordered)
1. **Constrain the memory model.** Give the RTOS a linker layout that keeps
   `_stack_top` inside the DMEM window (below 0x40000) so it does not alias;
   or make `data_bram` decode the full 256KB consistently and set region
   boundaries that don't cross the 0x40000/0x100000 aliases. Verify with a
   small heap first.
2. **Dump the actual TCB contents** before/after idle creation (uPriority,
   pxStack, ready-list linkage) to confirm exactly which field is corrupted and
   by what write.
3. Confirm the port's `xPortSetupTimerInterrupt` / first-tick path is not also
   involved (the idle spin never preempts, so a missing tick could be the
   second issue once dispatch is fixed).

## Repro & supporting files
- `hardware/sim/soc/tb_soc_rtos.cpp` — UART-capture harness; stages
  `firmware/rtos.hex` as `program.hex` (run_sim.sh rule added).
- `firmware/apps/rtos.c` — two-task FreeRTOS app (A prio 2, B prio 1).
- `firmware/apps/{mincase,loadbr}.c` — CPU load->branch micro-tests (all PASS).
- `firmware/boot/linker_rtos.ld` — two-region RTOS linker (heap in DMEM).
- `firmware/rtos/FreeRTOSConfig.h` — RTOS config (CLINT timer base
  0x0200BFF8 / 0x02004000; see `hardware/rtl/soc/timer.v`).
