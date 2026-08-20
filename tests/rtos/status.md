# FreeRTOS bring-up — dispatch bug FIXED

## Current state (end of fix session)
`make sim TB=soc/tb_soc_rtos` **PASSES**: banner prints, both xTaskCreate
calls succeed, `pxCurrentTCB` stays task A (prio 2) after B (prio 1) is
created, and the scheduler preempts both tasks — "task A tick 1..4" and
"task B (lower prio)" x2, then the TB exits on the required counts.

## Root cause (one paragraph)
`decode_stage.v`'s ID/EX capture gave the hazard `stall` priority over the
pipeline-freeze `hold`. With two back-to-back loads, the first load holds MEM
(`mem_hold` → `mem_stall`), the second load sits frozen in EX, and the
consumer branch in IF/ID triggers the load-use `stall` — so decode injected a
bubble into ID/EX, erasing the second load's `mem_read` before EX/MEM could
capture it. The priority is now `reset > hold > stall > flush > capture`, so a
frozen bundle survives and the hazard stall re-asserts on release.

## Fix commit summary
- `hardware/rtl/cpu/core/decode_stage.v`: `hold` now wins over `stall`.
- `hardware/sim/soc/tb_soc_rtos.cpp`: MAX_CYCLES 9M → 160M (9M @50 MHz is
  180 ms < one 500 ms task-A delay; the old budget could never pass).
- Cleaned up: previous session's debug `$display` in exec_stage.v had
  accidentally REPLACED the `ex_mem_alu_result` capture (zeroed every ALU
  result, broke boot); restored + removed prints.

## Ruled-out / do not re-try
1. `forwarding_net` change (don't forward a distance-1 load's address as
   data) — did NOT fix; the erased load, not the forward value, was the bug.
2. `mem_stage` hold counter rework — broke boot (mem_hold stayed high).
   `mem_hold` was never the problem: it fires exactly one cycle.

## Regression evidence (all after the fix)
- tb_cpu_top 83/83, tb_soc_gpu, tb_dual_spin (1001 increments), litmus
  MP/SB, compliance 53/54 (1 skip Zifencei), tb_soc_rtos PASS.

Full details in `tests/rtos/load_branch_bug.md`.
