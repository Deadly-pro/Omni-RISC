// Omni-RISC APU — timer demo
// Arms the machine timer; the interrupt handler toggles gpio_out[0].
#include "../drivers/timer.h"

// ~500k cycles between toggles at 50 MHz => visible on a scope/LED
#define PERIOD 100000u

void app_main(void) {
    timer_init(PERIOD);
    while (1);
}
