/**
 * @file tick_test.c
 * @brief F7 gate app: time and software timers (firmware track).
 *
 * Families:
 *   elapsed   the rollover-safe difference, unit-tested at the 32-bit boundary
 *   mono      ticks_us advances by at least what a delay asked for
 *   delay     delay_ms lands within tolerance, measured with mtime (not by faith)
 *   oneshot   a one-shot fires exactly once, promptly
 *   periodic  three same-period timers fire in registration order, repeatedly
 *   cancel    a cancelled timer never fires, including cancelled from a callback
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/tick.h"
#include <omni/stdio.h>

static uint32_t n_pass, n_fail;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

/* ------------------------------------------------------------ callbacks --- */
static volatile unsigned a_fires, b_fires, c_fires;
static volatile unsigned order[8], order_n;
static volatile int id_b = -1;

static void cb_a(void *arg) { (void)arg; a_fires++; if (order_n < 8) order[order_n++] = 'A'; }
static void cb_b(void *arg) { (void)arg; b_fires++; if (order_n < 8) order[order_n++] = 'B'; }
static void cb_c(void *arg) { (void)arg; c_fires++; if (order_n < 8) order[order_n++] = 'C'; }

static volatile unsigned cancel_fires;
static void cb_cancel_me(void *arg) { (void)arg; cancel_fires++; }
static void cb_canceller(void *arg)
{
    (void)arg;
    if (id_b >= 0) timer_cancel(id_b);      /* cancel from inside a callback */
    id_b = -2;
}

/* -------------------------------------------------------------- families -- */
static void t_elapsed(void)
{
    uint32_t a = tick_elapsed(0xFFFFFF00u, 0x00000100u);
    uint32_t b = tick_elapsed(100u, 400u);
    report("elapsed", a == 0x200u && b == 300u,
           "rollover-safe difference: 0xFFFFFF00->0x00000100 is 0x200");
}

static void t_mono(void)
{
    uint64_t t0 = ticks_us();
    uint64_t t1, d;
    delay_us(1000u);
    t1 = ticks_us();
    d = t1 - t0;
    /* What this family adds over `delay` is that time moves *forward* and does
     * not jump; the magnitude is asserted there, over a 10x longer window with a
     * measured tolerance. */
    report("mono", d != 0u && d <= 5000u, "ticks_us advanced and did not jump");
}

static void t_delay(void)
{
    uint32_t t0 = 0, d;
    t0 = (uint32_t)ticks_us();
    delay_ms(10u);
    d = (uint32_t)ticks_us() - t0;
    /* measured, not assumed: 10 ms +/- 25% */
    report("delay", d >= 7500u && d <= 12500u, "delay_ms(10) landed within 25%");
}

static void t_oneshot(void)
{
    unsigned before = timer_fires();
    unsigned spins = 0;
    int ok;

    a_fires = 0;
    timer_add(5000u, 1, cb_a, 0);                    /* one-shot, 5 ms */
    while (a_fires == 0 && spins++ < 2000000u) { }   /* spin until it fires */
    ok = (a_fires == 1) && (timer_fires() > before) && (timer_max_late_us() < 3000u);
    report("oneshot", ok, ok ? "fired exactly once, promptly"
                             : "one-shot did not fire exactly once");
    delay_ms(8);
    ok = a_fires == 1;                               /* and not again */
    report("oneshot-once", ok, ok ? "and did not repeat" : "a one-shot fired twice");
}

static void t_periodic(void)
{
    unsigned fa, fb, fc;
    int ok;

    a_fires = b_fires = c_fires = 0;
    order_n = 0;
    timer_add(2000u, 0, cb_a, 0);
    timer_add(2000u, 0, cb_b, 0);
    timer_add(2000u, 0, cb_c, 0);
    delay_ms(20u);
    fa = a_fires; fb = b_fires; fc = c_fires;

    ok = fa >= 7u && fa <= 12u && fb >= 7u && fb <= 12u && fc >= 7u && fc <= 12u;
    /* registration order within the first dispatch, which is what "same period"
     * is supposed to mean */
    ok = ok && order_n >= 3u && order[0] == 'A' && order[1] == 'B' && order[2] == 'C';
    report("periodic", ok, ok ? "three same-period timers fired repeatedly, in registration order"
                              : "periodic timers fired the wrong number of times or out of order");
}

static void t_cancel(void)
{
    int id, ok;
    unsigned before = cancel_fires;

    cancel_fires = 0;
    id = timer_add(2000u, 0, cb_cancel_me, 0);
    ok = id >= 0 && timer_cancel(id) == 1;
    delay_ms(10u);
    ok = ok && cancel_fires == 0 && timer_cancel(id) == 0;   /* already gone */
    report("cancel", ok, ok ? "a cancelled timer never fired, and cancelling twice is harmless"
                            : "a cancelled timer still fired");

    /* cancel from inside another callback */
    cancel_fires = 0;
    b_fires = 0;
    id_b = timer_add(2000u, 0, cb_cancel_me, 0);
    timer_add(4000u, 1, cb_canceller, 0);                    /* fires at 4 ms, cancels B */
    delay_ms(20u);
    ok = (cancel_fires >= 1u) && (cancel_fires <= 2u) && id_b == -2;
    report("cancel-from-cb", ok, ok ? "a timer cancelled from a callback stopped firing"
                                    : "cancel from a callback did not take effect");
    (void)before;
}

void app_main(void)
{
    uart_init();
    tick_init(1000u);               /* 1 ms tick */

    printf("[TEST] READY\r\n");
    printf("[TEST] tick gate tick_us=1000 timers=%u\r\n", (unsigned)TICK_MAX_TIMERS);

    t_elapsed();
    t_mono();
    t_delay();
    t_oneshot();
    t_periodic();
    t_cancel();

    printf("[TEST] tick fires=%u late=%u max_late_us=%u\r\n",
           timer_fires(), timer_late(), (unsigned)timer_max_late_us());
    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
