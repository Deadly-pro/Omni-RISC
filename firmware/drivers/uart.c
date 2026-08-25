// Omni-RISC APU — uart driver implementation
#include "uart.h"

#define UART_BASE   0x40000000
#define UART_TX     (*(volatile uint32_t *)(UART_BASE + 0x00))
#define UART_STATUS (*(volatile uint32_t *)(UART_BASE + 0x04))
#define UART_RXDATA (*(volatile uint32_t *)(UART_BASE + 0x08))
#define TX_BUSY     1
#define RX_READY    2
#define RX_OVERRUN  4

void uart_init(void) {
    // nothing to configure — baud fixed in RTL; reading RXDATA clears
    // a stale overrun flag left over from reset
    (void)UART_RXDATA;
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

uint32_t uart_status(void) {
    return UART_STATUS;
}

int uart_getchar(void) {
    if (!(UART_STATUS & RX_READY))
        return -1;
    return (int)(UART_RXDATA & 0xFF);   // read pops the FIFO, clears overrun
}
