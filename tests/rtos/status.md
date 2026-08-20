# FreeRTOS bring-up — persistent dispatch bug

## Current state (end of deep-dive session)
Everything builds and the kernel **boots to the scheduler**:
`xTaskCreate` succeeds for both tasks; `pxCurrentTCB` = 0x11190 (task B, prio 1)
right after creating A(prio 2) then B(prio 1). The scheduler therefore runs
task B / idle as the first task and no user-task body ever executes
("task A tick ..." never prints).

## Precise symptom
Firmware-side debug is unambiguous:
```
xTaskCreate A=1 B=1   hA=00010900(prio2)  hB=00011190(prio1)  curTCB=00011190(prio1)
```
`pxCurrentTCB` should remain task A (prio 2) after B (prio 1) is created:
the creator does `if(pxCurrentTCB->uxPriority <= pxNewTCB->uxPriority)` =
`2 <= 1` = false → keep A. The CPU instead makes it B, i.e. the priority
compare branch resolves wrong. This is **not** a FreeRTOS logic bug — it is a
CPU hazard/timing bug in the registered-read data path for back-to-back loads.

## What was ruled out (CPU is otherwise verified)
- riscv-arch-test compliance: still 53/54.
- tb_cpu_top 83/83 (incl. load-use interlock + single-load->beq).
- L1 caches, SoC-GPU, dual-core spinlock + litmus: all pass.
- Hand micro-tests (`firmware/apps/mincase.c`) show the load->branch compare is
  **flaky across gcc -Os register allocations**: the same source sometimes
  passes, sometimes fails, confirming a marginal timing/hazard issue that only
  fires for specific back-to-back `lw;lw;branch` instruction layouts.

## Attempted fixes (all reverted; core kept pristine)
1. **Forwarding change** (`forwarding_net.v`: don't forward a distance-1 load's
   `ex_mem_alu_result` — its address — as data). Theoretically correct
   (a load's data is only valid at MEM/WB), but did NOT fix the micro-test.
2. **Registered-read hold counter** (`mem_stage.v` g_nocache: per-load `ld_st`
   counter instead of single `dbram_hold` toggle). BROKE BOOT (pipeline froze:
   boot banner never printed), so reverted.

Neither change is in the tree; `hardware/rtl/cpu/core/` is at commit 94c8358.

## Leading hypothesis (next-session starting point)
The single-cycle load-use hazardous stall + the single `dbram_hold` toggle do
not correctly cover a **2-cycle-latency registered BRAM read** when two loads
are back-to-back and a branch consumes both results. The correct fix needs the
pipeline to hold the load until its data is truly valid in MEM/WB, coordinated
with the hazard stall — my counter attempt was on the right track but its
`mem_hold` stayed high (froze boot); the counter transition/load-identity
logic must be corrected. Best validated against the **deterministic** RTOS
run (always fails identically) and the full regression, using a wave/parse on
`load_rdata`/`mem_wb_rdata`/`ex_mem_mem_read`/`mem_hold` (all already marked
public in a working VCD path — see notes below).

## Working diagnostics you can trust
- Firmware prints (`apps/rtos.c`): xTaskCreate results, task handles,
  priorities, and pxCurrentTCB — reliable.
- `tests/rtos/status.md` (original), `tests/rtos/load_branch_bug.md`.
- VCD path verified: `tb_soc_rtos.cpp` can dump a VCD; `$var` lines are
  `$var wire <width> <id> <ref> [range] $end`; multi-bit body lines are
  `b<val><id>`, single-bit `0<id>`. Signals like `u_mem.mem_hold(S!)`,
  `u_mem.load_rdata`, `u_wb.mem_wb_rdata(z!)` are traceable.
