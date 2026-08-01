// Omni-RISC APU — timer driver header
#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>

// Program the machine timer to interrupt every `period` cycles and enable
// machine timer interrupts (mie.MTIE + mstatus.MIE).
void timer_init(uint32_t period);

// C handler called from the assembly trap trampoline. mcause is passed in a0.
// Toggles gpio_out[0] and advances mtimecmp on a machine timer interrupt.
void timer_int_handler(uint32_t mcause);

#endif
