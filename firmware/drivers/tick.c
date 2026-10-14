/* Omni-RISC APU - time and software timers (firmware track, F7).
 * Spec: firmware/drivers/tick.h.
 */
#include "tick.h"

#define MTIME_LO (*(volatile uint32_t *)0x0200BFF8u)
#define MTIME_HI (*(volatile uint32_t *)0x0200BFFCu)
#define MTIMECMP_LO (*(volatile uint32_t *)0x02004000u)
#define MTIMECMP_HI (*(volatile uint32_t *)0x02004004u)

#define TICKS_PER_US 50u                /* 50 MHz */

typedef struct {
    uint32_t next;                      /* absolute low-word deadline */
    uint32_t period_us;
    void   (*cb)(void *);
    void    *arg;
    int      used;
    int      one_shot;
} timer_t;

static timer_t  timers[TICK_MAX_TIMERS];
static int      enabled;
static uint32_t tick_us;
static uint32_t base_us, last_lo;        /* accumulate microseconds between ticks */
static unsigned fires, late;
static uint32_t max_late;

/* 64-bit mtime, read so that a carry between the two words cannot be seen. */
static uint64_t mtime(void)
{
    uint32_t hi, lo, hi2;
    do {
        hi = MTIME_HI;
        lo = MTIME_LO;
        hi2 = MTIME_HI;
    } while (hi != hi2);
    return ((uint64_t)hi << 32) | lo;
}

static void arm(uint32_t us_from_now)
{
    uint64_t t = mtime() + (uint64_t)us_from_now * TICKS_PER_US;
    MTIMECMP_LO = (uint32_t)t;
    MTIMECMP_HI = (uint32_t)(t >> 32);
}

uint32_t tick_elapsed(uint32_t t0, uint32_t t1)
{
    return t1 - t0;                     /* unsigned: correct across a rollover */
}

/* Microseconds as a 32-bit count (wraps every ~71 min, which is plenty for this
 * service): a base accumulated by the tick ISR plus the part of the current tick
 * that has already elapsed. Deliberately NOT mtime()/50 - that is a 64-bit
 * divide, and this link has no libgcc, so it would be an undefined __udivdi3. */
static uint32_t us_now(void)
{
    return base_us + (uint32_t)((MTIME_LO - last_lo) / TICKS_PER_US);
}

uint64_t ticks_us(void) { return (uint64_t)us_now(); }
uint32_t ticks_ms(void) { return us_now() / 1000u; }
int tick_enabled(void) { return enabled; }

void tick_init(uint32_t period_us)
{
    for (unsigned i = 0; i < TICK_MAX_TIMERS; i++) timers[i].used = 0;
    tick_us = period_us ? period_us : 1000u;
    fires = late = max_late = 0;
    base_us = 0;
    last_lo = MTIME_LO;
    enabled = 1;
    /* Arming mtimecmp is not enough: without mie.MTIE and mstatus.MIE the trap
     * never arrives and every callback silently never runs. */
    __asm__ volatile("csrs mie, %0"     :: "r"(0x80u));   /* MTIE */
    __asm__ volatile("csrs mstatus, %0" :: "r"(0x8u));    /* MIE  */
    arm(tick_us);
}

int timer_add(uint32_t period_us, int one_shot, void (*cb)(void *), void *arg)
{
    for (int i = 0; i < TICK_MAX_TIMERS; i++) {
        if (timers[i].used) continue;
        timers[i].used = 1;
        timers[i].period_us = period_us;
        timers[i].cb = cb;
        timers[i].arg = arg;
        timers[i].one_shot = one_shot;
        timers[i].next = MTIME_LO + period_us * TICKS_PER_US;
        return i;
    }
    return -1;
}

int timer_cancel(int id)
{
    if (id < 0 || id >= TICK_MAX_TIMERS || !timers[id].used) return 0;
    timers[id].used = 0;
    return 1;
}

void tick_isr(void)
{
    uint32_t now = MTIME_LO;

    base_us += tick_us;                 /* one tick of wall time has passed */
    last_lo = now;

    for (int i = 0; i < TICK_MAX_TIMERS; i++) {
        timer_t *t = &timers[i];
        if (!t->used) continue;
        if ((int32_t)(now - t->next) < 0) continue;         /* not due yet */
        {
            uint32_t l = tick_elapsed(t->next, now);
            if (l) { late++; if (l > max_late) max_late = l; }
        }
        fires++;
        if (t->one_shot) t->used = 0;
        else t->next += t->period_us * TICKS_PER_US;
        if (t->cb) t->cb(t->arg);
    }
    arm(tick_us);                                            /* re-arm the tick */
}

void delay_us(uint32_t us)
{
    uint32_t start = MTIME_LO;
    uint32_t want = us * TICKS_PER_US;
    while ((uint32_t)(MTIME_LO - start) < want)
        ;
}

void delay_ms(uint32_t ms) { delay_us(ms * 1000u); }

unsigned timer_fires(void) { return fires; }
unsigned timer_late(void) { return late; }
uint32_t timer_max_late_us(void) { return max_late / TICKS_PER_US; }
