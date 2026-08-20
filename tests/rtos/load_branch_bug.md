# Core bug: unsigned branch fed by a back-to-back load mis-evaluates

## Status
**OPEN — reproducible on the RV32IM core in Verilator (2026-08-20).** Found
during the FreeRTOS bring-up, which exposed it because the kernel's scheduler
does priority-compare branches fed straight from memory loads.

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
The wrong evaluation makes the scheduler think the **idle task (prio 0)
outranks task A (prio 2)**, so `xPortStartFirstTask` launches idle and the
RTOS never runs a user task. That was the actual first bug behind the
"scheduler runs idle forever" symptom.

## Root-cause hypothesis (not yet confirmed at signal level)
`data_bram`'s read is **registered** (`rdata <= mem[addr]`, Phase G BRAM fix),
so a load's value is only valid in MEM/WB, not EX/MEM. The pipeline stalls the
load-use consumer for **one** cycle (hazard_unit), and `forwarding_net`
forwards distance-1 producers as `ex_mem_alu_result`. For a load this is the
ALU's stale output, not the loaded data; the one-cycle stall is insufficient
to cover the registered read's extra latency. Single-load `tb_cpu_top` CASE D
passes (its `beq` uses the load result as `rs2`, and the mem_hold freezes the
pipe differently), but the two-back-to-back-load pattern (dist-2 producer in
flight on the same cycle as the dist-1 load) misses the stall and forwards a
stale value into the branch comparator.

## Fix direction (validate before applying)
1. Make `forwarding_net` **not** forward `ex_mem_alu_result` when the distance-1
   producer is a load (`ex_mem_mem_read`): a load's data is not ready in
   EX/MEM.
2. Make `hazard_unit` stall **two** cycles (or until the load reaches
   MEM/WB) when the consumer target is a load — the registered read has an
   extra cycle of latency that the current single-cycle stall does not cover.

Fix candidates must be regression-checked against `tb_cpu_top` (83/83),
`tb_soc_gpu`, `tb_dual_spin`, and compliance before landing.

## Repro files
- `firmware/apps/branchtest.c` — compiler-based battery (first version was
  partially constant-folded; keep the volatile/runtime-value forms).
- `firmware/apps/loadbr.c` — minimal hand-assembled deterministic repro.
- `hardware/sim/soc/tb_soc_rtos.cpp` — UART-capture harness; stage any hex as
  `program.hex` in `obj_dir_tb_soc_rtos/`.
