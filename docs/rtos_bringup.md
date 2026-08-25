# RTOS Bring-up — FreeRTOS on Omni-RISC (Verilator)

Goal: run FreeRTOS (a real, preemptive RTOS) on the Omni-RISC RV32IM CPU in
Verilator, then measure scheduler metrics (context-switch cost, tick jitter,
ISR latency). This is the "port a real OS to my own core" story.

This doc is the working log + Phase 0 cross-check. Phases map to AGENTS.md /
roadmap convention.

## Phase 0 — port study + toolchain smoke test  (DONE)

Vendored **FreeRTOS Kernel V10.5.1** into `firmware/third_party/FreeRTOS`
(MIT; pinned tag, `.git` removed — a solo, offline repo uses vendored tree not
a submodule). Chosen over V11/HEAD because V10.5.1's `portable/GCC/RISC-V`
port is the simplest modern M-mode port and doesn't require the newer chip
config system.

Port files under `firmware/third_party/FreeRTOS/portable/GCC/RISC-V/`:
- `port.c` — `vPortSetupTimerInterrupt()`, `xPortStartScheduler()`, ISR stack.
- `portASM.S` — trap handler (`freertos_risc_v_trap_handler`), context
  save/restore, `pxPortInitialiseStack`, `xPortStartFirstTask`.
- `portContext.h` — save/restore macros (pulled into portASM.S).
- `portmacro.h` — port config, derives mtime/mtimecmp addresses.
- `chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions/` —
  `portasmHAS_MTIME=1`, `portasmHAS_SIFIVE_CLINT=1`, no extra registers.
  The assembler include path must point at this dir (it's how the port knows
  which chip it's on) — see build flags below.

### CSR / RTL cross-check (each FreeRTOS requirement vs hardware/rtl)

| FreeRTOS needs | RTL support | Verdict |
|---|---|---|
| `mstatus.MIE/MPIE` (bits 3,7), write via `csrw mstatus` | `csr_file.v` 0x300 write captures bits 3 & 7 | OK — MPP hardwired to M-mode (2'b11) |
| `mie.MTIE`/`MEIE`, `csrs mie,0x880` | `csr_file.v` 0x304 write `mie_mtie`,`mie_meie` | OK |
| `mtvec` = trap handler | we set in boot; `mtvec_o` word-masked | OK |
| `mepc` write (`csrw mepc`, 0x341) | `csr_file.v` 0x341 | OK |
| `mcause` read (0x342) — 0x80000007 timer, 11 ecall | `trap_unit.v` produces exactly these | OK |
| `mhartid` read (0xF14) | `csr_file.v` F14 → 0 (single-hart) | OK |
| `mret` | `trap_unit.v` + csr pop | OK |
| CLINT: mtime @ 0x0200_BFF8, mtimecmp @ 0x0200_4000 | `timer.v` regions +0xBFF8 / +0x4000 | OK — exact match |
| Timer interrupt gating | `trap_unit.v`: `mstatus_mie & mie_mtie & mip_mtip` | OK |

The trap handler replaces our current `boot/trap_handler.S` (which only handles
timer). FreeRTOS's `freertos_risc_v_trap_handler` dispatches on `mcause`:
- timer (0x80000007) → arm next compare + `xTaskIncrementTick` (+ maybe
  `vTaskSwitchContext`)
- ecall (11) → `vTaskSwitchContext` (this is how `taskYIELD()` works)
- everything else → weak `freertos_risc_v_application_exception_handler`
  (defaults to an infinite loop — a debug hook for us)

### Toolchain smoke test (DONE — all compile clean)

Flags (must match firmware Makefile): `-march=rv32im_zicsr_zifencei
-mabi=ilp32 -Os -Wall -ffreestanding -fno-builtin`.

Key findings:
1. **No rv32 libc installed.** `-ffreestanding` gives only GCC builtin headers;
   FreeRTOS needs `<stdlib.h>` (abort) and `<string.h>`
   (memcpy/memset/strlen). Added minimal freestanding shims in
   `firmware/rtos/include/{stdlib,string}.h` + `firmware/rtos/omni_libc.c`.
2. `portASM.S` must be assembled **with** `-march=rv32im_zicsr_zifencei` (not
   bare `rv32im`) because it emits CSR ops — `zicsr` required, else the
   assembler errors on every `csrr`.
3. Config lives in `firmware/rtos/FreeRTOSConfig.h` (CLINT addresses,
   tick rate, heap, ISR stack).

`FreeRTOSConfig.h` notes:
- `configCPU_CLOCK_HZ 50000000`, `configTICK_RATE_HZ 1000` → 50k cycles/tick.
- `configISR_STACK_SIZE_WORDS 128` → port statically allocates the ISR stack
  (avoids needing a `__freertos_irq_stack_top` linker symbol).
- `configSUPPORT_DYNAMIC_ALLOCATION 1` + `heap_4.c`.
## Phase 1 — port bring-up  (DONE)

Two tasks (A prio 2, B prio 1) + vTaskDelay; UART-decoding TB
(tb_soc_rtos) captures banner, both xTaskCreate calls, scheduler start,
and correct preemption. Gate: PASS.

## Phase 2 — scheduler validation  (DONE)

- tb_soc_rtos PASS: task A ticks 1-4 at exactly 25,050,000 cycles apart
  (500ms @ 50MHz, 50k cycles/tick), task B prints at 60,085,000 (1200ms).
  `pxCurrentTCB` stays task A (prio 2) after B (prio 1) is created.
- This milestone exposed a core pipeline bug (load;load;branch mis-eval);
  fixed in decode_stage.v (hold > stall priority) — see
  tests/rtos/load_branch_bug.md. Regression: tb_cpu_top 83/83, tb_soc_gpu,
  tb_dual_spin, litmus MP/SB, compliance 53/54.
- Queue producer/consumer: DONE (Phase 2a) — see below.
- The tick hook (configUSE_TICK_HOOK=1) exposed a second core bug (see
  Phase 4) fixed in exec_stage.v + trap_unit.v.

## Phase 2a — queues  (DONE)

tb_soc_rtos_q PASS: producer (prio 1) sends 1..1000 into a depth-8
xQueue; consumer (prio 2) verifies strict order. 4 progress reports,
"QUEUE DONE 1000" at 2,459,685 cycles. App: firmware/apps/rtos_q.c.
Firmware Makefile links FreeRTOS for rtos/rtos_q/rtos_sem/rtos_metrics.

## Phase 3 — interrupt depth  (DONE)

Machine-software-interrupt path: CLINT msip @ 0x0200_0000 (timer.v) →
csr_file.mip_msip → trap_unit takes mcause 0x80000003 (MSI priority over
MTI). tb_soc_rtos_sem PASS: vLatTrigTask raises msip 3x, ISR handler gives
a semaphore, waiter counts 3/3 at exactly 200ms spacing. Gotcha: the port
only enables MTIE/MEIE, so firmware must `csrs mie, 0x8` (rtos_sem.c
app_main) or msip pends forever.

## Phase 4 — numbers + docs  (DONE)

tb_soc_rtos_metrics PASS (app firmware/apps/rtos_metrics.c, mtime-stamped
via the CLINT's free-running 64-bit counter):
- ISR entry latency (msip write → first handler instruction): 20 samples,
  avg 92 cycles (1.84µs @ 50MHz).
- 2-switch taskYIELD round trip (A→B→A): 200 trips, avg 492 cycles
  (9.84µs).
- Tick jitter (vApplicationTickHook deltas vs ideal 50,000): range
  49,806–50,192 over 1200+ ticks (max deviation 194 cycles).

configUSE_TICK_HOOK is 1 kernel-wide; rtos/omni_libc.c carries a weak
empty vApplicationTickHook default, apps override with a strong one.

### Second core bug: interrupt during redirect / load-forwarding

Enabling the tick hook changed the kernel's code layout and exposed a new
pipeline bug (the queue test would corrupt a task's saved ra and return to
garbage, or — after the first fix — deliver wrong queue items). Root cause:
an interrupt taken while (a) a branch/jump redirect was resolving, or (b) a
load sat in MEM with its forwarded value feeding the EX instruction,
captured an inconsistent (mepc, register-file) pair:

- (a) redirect_pending included the same-cycle redirect_valid, so trap_pc
  muxed in the REGISTERED redirect_target_q, which still held the PREVIOUS
  redirect's target → mret jumped into a stale pc (rtos_q BADREDIR at the
  tail-call `j` of vTaskPlaceOnEventList). Fix: redirect_pending is now the
  drain window only (redirect_win != 0); on the redirect cycle mepc =
  id_ex_pc (the branch itself), so mret re-executes it.
- (b) int_window accepted interrupts while a load was in MEM; if the EX
  instruction consumed its forwarded value, the trap flush lost the value
  and the resume re-executed the instruction against a stale register file
  → queue ORDER FAIL. Fix: int_window &= ~ex_mem_mem_read (defer while a
  MEM-stage load is in flight; ex_mem_mem_read is a register so no
  combinational loop with the trap→flush path).

Regression after both fixes: tb_cpu_top PASS, tb_dual_spin PASS, rtos_q
PASS (2,464,006 cycles), rtos/rtos_sem/rtos_metrics PASS.
