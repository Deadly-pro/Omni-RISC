// Omni-RISC APU — uart driver header
#ifndef UART_H
#define UART_H

#include <stdint.h>

void uart_init(void);
void uart_putc(char c);
void uart_puts(const char *s);

// RX (FIFO-buffered in RTL): -1 if empty, else the next byte (pops).
int  uart_getchar(void);
// Raw STATUS: bit0 TX busy, bit1 RX ready, bit2 RX overrun (sticky).
uint32_t uart_status(void);

#endif
