// Omni-RISC APU — UART RX echo app (R2 gate).
//
// Prints a ready banner, then echoes every received byte back over TX.
// Reports (once) if the RTL flags an RX FIFO overrun. The tb_soc_uart_rx
// testbench drives uart_rx bit-banged and asserts on the decoded TX stream.
#include "../drivers/uart.h"

void app_main(void) {
    uart_init();
    uart_puts("[ECHO] ready\n");

    uint32_t overrun_seen = 0;
    for (;;) {
        uint32_t st = uart_status();
        if ((st & 0x4) && !overrun_seen) {
            uart_puts("[ECHO] overrun\n");
            overrun_seen = 1;
        }
        int c = uart_getchar();
        if (c >= 0)
            uart_putc((char)c);
    }
}
