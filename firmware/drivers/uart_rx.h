/* Omni-RISC APU - software receive path for the RTL UART (firmware track, F3).
 *
 * The RTL hands you a 16-byte RX FIFO with a sticky overrun flag and no
 * interrupt line, so the engineering lives here: a ring buffer the pump keeps
 * topped up, an explicit policy for a full ring, and blocking calls with
 * timeouts instead of poll-and-hope.
 *
 * Single context by design: with no RX interrupt, the pump, the readers and the
 * counters are all reachable from one thread of control on this SoC, so no
 * locking is needed - worth knowing rather than assuming.
 *
 * Timeouts are in system clock ticks (50 MHz on this SoC; F7 adds ms helpers).
 */
#ifndef UART_RX_H
#define UART_RX_H

#include <stdint.h>

#define UART_RX_RING    256u        /* must stay a power of two */
#define UART_RX_TIMEOUT (-1)        /* uart_rx_getline's "gave up" return */

void     uart_rx_init(void);
void     uart_rx_pump(void);        /* drain the RTL FIFO into the ring */
unsigned uart_rx_available(void);
int      uart_rx_getc(void);        /* -1 when empty */
unsigned uart_rx_read(uint8_t *dst, unsigned n);
/* Reads up to n-1 bytes, NUL-terminates, returns the length, or UART_RX_TIMEOUT.
 * A bare CR (CRLF) is treated as part of the terminator. */
int      uart_rx_getline(char *dst, unsigned n, uint32_t timeout);

/* Counters. The RTL FIFO's losses are invisible to software, which is exactly
 * why they have to be counted from the sticky flag: report, never pretend. */
unsigned uart_rx_bytes(void);       /* bytes staged into the ring */
unsigned uart_rx_dropped(void);     /* ring was full, so the byte was dropped */
unsigned uart_rx_overruns(void);    /* sticky RTL overrun observed */
void     uart_rx_clear_stats(void);

#endif /* UART_RX_H */
