// Omni-RISC APU — timer driver implementation
#include "timer.h"
#include "gpio.h"

#define TIMER_BASE    0x02000000
#define MTIMECMP_LO   (*(volatile uint32_t *)(TIMER_BASE + 0x4000))
#define MTIMECMP_HI   (*(volatile uint32_t *)(TIMER_BASE + 0x4004))
#define MTIME_LO      (*(volatile uint32_t *)(TIMER_BASE + 0xBFF8))
#define MTIME_HI      (*(volatile uint32_t *)(TIMER_BASE + 0xBFFC))

static uint32_t period;

static uint64_t timer_now(void) {
    return ((uint64_t)MTIME_HI << 32) | MTIME_LO;
}

static void timer_set_cmp(uint64_t cmp) {
    MTIMECMP_LO = (uint32_t)cmp;
    MTIMECMP_HI = (uint32_t)(cmp >> 32);
}

void timer_init(uint32_t p) {
    period = p;
    timer_set_cmp(timer_now() + period);

    // enable machine timer interrupt: mie.MTIE (bit7) and global mstatus.MIE
    asm volatile("csrs mie, %0" ::"r"((uint32_t)(1u << 7)));
    asm volatile("csrs mstatus, %0" ::"r"((uint32_t)(1u << 3)));
}

void timer_int_handler(uint32_t mcause) {
    // machine timer interrupt => mcause == 0x80000007
    if ((mcause & 0x80000000u) && ((mcause & 0xffu) == 7u)) {
        gpio_write(gpio_read() ^ 1u);      // toggle gpio_out[0]
        timer_set_cmp(timer_now() + period); // schedule the next interrupt
    }
}
