// Omni-RISC APU — uart driver implementation
#include "uart.h"

#define UART_BASE   0x40000000
#define UART_TX     (*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_STATUS (*(volatile uint32_t *)(UART_BASE + 0x04))
#define TX_BUSY     1

void uart_init(void) {
    // nothing to configure — TX-only, baud fixed in RTL
}

void uart_putc(char c) {
    while (UART_STATUS & TX_BUSY)
        ;
    UART_TX = (uint32_t)(uint8_t)c;
}

void uart_puts(const char *s) {
    while (*s)
        uart_putc(*s++);
}
