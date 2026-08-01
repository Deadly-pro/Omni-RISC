// Omni-RISC APU — Boot sequence
//  - initialize UART
//  - set the trap vector (mtvec) to the assembly trampoline
//  - print the boot banner
//  - hand off to the application
#include <stdint.h>
#include "../drivers/uart.h"

// Provided by boot/trap_handler.S
extern void _trap_handler(void);

// Provided by the application (apps/<app>.c)
void app_main(void);

int main(void) {
    uart_init();

    // point mtvec at the trap trampoline (vector base, direct mode)
    asm volatile("csrw mtvec, %0" ::"r"((uint32_t)_trap_handler));

    uart_puts("Omni-RISC APU v1.0\n");

    app_main();

    while (1);
    return 0;
}
