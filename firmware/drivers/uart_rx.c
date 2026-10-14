/* Omni-RISC APU - UART software receive path (firmware track, F3).
 * Spec: firmware/drivers/uart_rx.h.
 */
#include "uart_rx.h"
#include "uart.h"

/* mtime, low word (CLINT: 0x0200_0000 + 0xBFF8) - used only for timeouts */
#define MTIME_LO (*(volatile uint32_t *)0x0200BFF8u)

static uint8_t  ring[UART_RX_RING];
static unsigned head, tail;
static unsigned n_bytes, n_dropped, n_overruns;

static uint32_t now(void) { return MTIME_LO; }

void uart_rx_init(void)
{
    head = tail = 0;
    n_bytes = n_dropped = n_overruns = 0;
    /* Deliberately does NOT call uart_init(): that reads RXDATA to clear the
     * reset-time sticky overrun, and reading RXDATA pops a byte. Boot clears it
     * before any input exists; doing it here would eat the first character. */
}

unsigned uart_rx_bytes(void)    { return n_bytes; }
unsigned uart_rx_dropped(void)  { return n_dropped; }
unsigned uart_rx_overruns(void) { return n_overruns; }

void uart_rx_clear_stats(void)  { n_bytes = n_dropped = n_overruns = 0; }

void uart_rx_pump(void)
{
    for (;;) {
        /* STATUS first, every time: reading RXDATA clears the sticky overrun
         * bit, so an overrun that happens mid-drain would otherwise vanish. */
        uint32_t st = uart_status();
        if (st & 4u) n_overruns++;
        if (!(st & 2u)) break;                 /* nothing ready */
        int c = uart_getchar();
        if (c < 0) break;
        unsigned next = (head + 1u) & (UART_RX_RING - 1u);
        if (next == tail) {                    /* ring full: count and drop */
            n_dropped++;
            continue;
        }
        ring[head] = (uint8_t)c;
        head = next;
        n_bytes++;
    }
}

unsigned uart_rx_available(void)
{
    return (head - tail) & (UART_RX_RING - 1u);
}

int uart_rx_getc(void)
{
    if (head == tail) return -1;
    int c = ring[tail];
    tail = (tail + 1u) & (UART_RX_RING - 1u);
    return c;
}

unsigned uart_rx_read(uint8_t *dst, unsigned n)
{
    unsigned got = 0;
    while (got < n) {
        int c = uart_rx_getc();
        if (c < 0) break;
        dst[got++] = (uint8_t)c;
    }
    return got;
}

int uart_rx_getline(char *dst, unsigned n, uint32_t timeout)
{
    uint32_t start = now();
    unsigned i = 0;

    if (n == 0) return 0;
    for (;;) {
        int c;
        uart_rx_pump();
        c = uart_rx_getc();
        if (c >= 0) {
            /* CRLF: CR is part of the terminator, not line content, and it must
             * not end the line - returning on CR leaves the LF behind, and the
             * next call then reports an empty line. */
            if (c == '\r') continue;
            if (c == '\n') {
                dst[i] = '\0';
                return (int)i;
            }
            if (i + 1u < n) dst[i++] = (char)c;          /* keep room for NUL */
            continue;
        }
        /* rollover-safe elapsed test: subtract, then compare as unsigned */
        if (timeout != 0u && (uint32_t)(now() - start) >= timeout) {
            dst[i] = '\0';
            return UART_RX_TIMEOUT;
        }
    }
}
