/* Omni-RISC APU - time and software timers (firmware track, F7).
 *
 * Everything here is built on the CLINT's 64-bit mtime (0x0200_0000 + 0xBFF8),
 * which keeps counting whether anyone is looking or not. Two consequences worth
 * designing around, and both are exercised by the gate:
 *
 *   - the low word rolls over every ~86 s at 50 MHz, so every elapsed-time test
 *     is an unsigned subtraction, never a comparison of absolute values;
 *   - this core has no wfi, so "delay" means spin. That is honest, and it is why
 *     delays are a last resort next to a timer callback.
 *
 * The timer service is driven from the machine timer interrupt: tick_isr()
 * re-arms mtimecmp and runs whatever is due. Interrupts are routed to it by
 * boot/fault.c's trap_dispatch, and only when tick_init() has enabled it - an app
 * that programs the CLINT by hand (firmware/apps/timer.c) keeps its own path.
 */
#ifndef TICK_H
#define TICK_H

#include <stdint.h>

#define TICK_MAX_TIMERS 8

void     tick_init(uint32_t period_us);   /* enable the service, arm the first tick */
int      tick_enabled(void);
void     tick_isr(void);                  /* called from the trap path on MTI */

uint32_t ticks_ms(void);
uint64_t ticks_us(void);
uint32_t tick_elapsed(uint32_t t0, uint32_t t1);   /* t1 - t0, rollover-safe */

void     delay_us(uint32_t us);
void     delay_ms(uint32_t ms);

/* Returns an id >= 0, or -1 when the table is full. A one-shot frees its slot
 * when it fires; a periodic timer keeps it until cancelled. */
int      timer_add(uint32_t period_us, int one_shot, void (*cb)(void *), void *arg);
int      timer_cancel(int id);

unsigned timer_fires(void);         /* expiries dispatched */
unsigned timer_late(void);          /* expiries that were already overdue */
uint32_t timer_max_late_us(void);   /* worst lateness seen, in us */

#endif /* TICK_H */
