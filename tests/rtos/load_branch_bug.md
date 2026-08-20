# Core bug: unsigned branch fed by a back-to-back load mis-evaluates

## Status
**FIXED (2026-08-20) — root cause confirmed at signal level, fix landed.**
FreeRTOS on Omni-RISC now boots and preempts both tasks (tb_soc_rtos PASS,
task A ticks 1-4, task B prints twice). Full regression green: tb_cpu_top
83/83, tb_soc_gpu, tb_dual_spin, litmus MP/SB, compliance 53/54.

## Symptom
A `bgeu`/`bltu` (unsigned branch) whose operands come from **two consecutive
data-bram loads** clears or sets the wrong way when one of the loaded values
is used as the branch's `rs1`.

Deterministic minimised repro (hand-assembled, no compiler folding):

```
mem[0] = 2;  /* a */
mem[1] = 0;  /* b */

lw  t0, 0(a0)   ; t0 = a = 2   (dist-2 from branch)
lw  t1, 0(a1)   ; t1 = b = 0   (dist-1 from branch, branch rs1)
bgeu t1, t0     ; 0 >= 2  →  FALSE, expect no-take, but the CPU TAKES it
```

`hardware/sim/soc/tb_soc_rtos.cpp` + `firmware/apps/loadbr.c` reproduce it:
`do_branch` (2>=0, correctly taken) returns 1, `do_bgeu_v2` and
`do_branch_rs1_load` (0>=2, must be no-take) wrongly return 1.

## What it breaks
Any `load; load; bgeu/bltu` where the immediately-preceding load feeds `rs1`.
FreeRTOS's `prvAddNewTaskToReadyList` does exactly this:
```
lw a4, 44(a5)   ; pxCurrentTCB->uxPriority
lw a5, 44(s0)   ; pxNewTCB->uxPriority
bltu a5, a4     ; pick the highest-priority task
```
The wrong evaluation made the scheduler think the **idle task (prio 0)
outranks task A (prio 2)**, so `xPortStartFirstTask` launched idle and the
RTOS never ran a user task.

## Root cause (confirmed at signal level)
`data_bram`'s read is **registered** (`rdata <= mem[addr]`, Phase G BRAM fix),
so a load's value is only valid in MEM/WB, not EX/MEM. To cover the extra
latency, `mem_stage`'s g_nocache path holds the load in MEM for one cycle via
the `dbram_hold` toggle, driving `mem_stall` → `freeze`.

The failing window is a **load holding MEM while a second load sits in EX
and its consumer (the branch) sits in IF/ID**:

- Cycle N: L1 in MEM, `mem_hold=1` → `mem_stall=1` (pipeline frozen).
  L2 (load) in EX, branch in IF/ID.
- `hazard_unit` sees the branch's `rs1 == L2.rd` and L2 `mem_read=1` →
  asserts the load-use `stall`.
- `decode_stage`'s capture block had `if (reset || stall)` **before** the
  `hold` branch: it injected a bubble into ID/EX **even though the pipeline
  was frozen**. L2's `ex_mem_mem_read` (and reg_write) were cleared before
  EX/MEM could ever capture them; the branch then compared a stale/zero
  value. `mem_hold` itself stays high exactly one cycle, so this was purely
  a priority bug in `decode_stage`, not a hold-counter bug.

Previous attempts ruled out (do not re-try):
1. `forwarding_net` change (don't forward `ex_mem_alu_result` when distance-1
   producer is a load) — did NOT fix; the forward value was correct, the
   load itself was being erased.
2. `mem_stage` hold counter rework — broke boot (mem_hold stayed high).

## The fix
`hardware/rtl/cpu/core/decode_stage.v` — give `hold` priority over `stall`:

```
if (reset)         → bubble
else if (hold)     → hold ID/EX bundle as-is   (was: else if (hold) after stall)
else if (stall)    → hazard load-use bubble
else if (flush)    → redirect bubble
else               → capture
```

A pipeline freeze (div_stall / icache_miss / mem_hold) means the EX/MEM
capture is deferred, so the ID/EX bundle must survive the cycle. The hazard
stall re-asserts once the freeze releases, and L2 then advances to MEM with
its `mem_read` intact. This is the same principle as the existing
"hold over flush" fix (jump link value preservation), extended to the hazard
stall.

A secondary session artifact was also removed: the previous debug session's
`$display` had accidentally *replaced* the `ex_mem_alu_result` capture in
`exec_stage.v` (a debug print inline at L206), silently zeroing every ALU
result and breaking boot (`result=4` instead of `0x40000004`). The capture
line was restored; `$display` debug lines were removed entirely.

## Validation
- `make sim TB=soc/tb_soc_rtos` — PASS: banner, both xTaskCreate calls,
  `curTCB=00010900` (task A, prio 2) retained, task A ticks 1-4 + task B
  prints. NOTE: MAX_CYCLES was raised 9M → 160M in tb_soc_rtos.cpp; 9M
  cycles at 50 MHz is only 180 ms, less than one 500 ms task-A delay.
- `make sim TB=cpu/tb_cpu_top` — 83/83.
- `make sim TB=soc/tb_soc_gpu` — GPU vector-add PASS.
- `make sim TB=soc/tb_dual_spin` — 1001 increments, no lost updates.
- `make sim TB=soc/tb_litmus_mp` / `tb_litmus_sb` — PASS.
- `make compliance` — 53/54 (1 skip, Zifencei).

## Repro files
- `firmware/apps/branchtest.c` — compiler-based battery.
- `firmware/apps/loadbr.c` — minimal hand-assembled deterministic repro.
- `hardware/sim/soc/tb_soc_rtos.cpp` — UART-capture harness.
