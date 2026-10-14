/**
 * @file uart_rx_test.c
 * @brief F3 gate app: the software receive path (firmware track).
 *
 * Built in one of two modes by tests/test_uart.sh (via APP_DEFS), because the
 * two things worth proving need opposite host pacing:
 *
 *   -DUART_RX_BURST   the host fires a running counter pattern with no pacing
 *                     (TB_PACE=burst). The 16-byte RTL FIFO *will* overrun: the
 *                     app deliberately stalls first, then drains, and must
 *                     account for every byte - consumed plus the loss it can
 *                     reconstruct from the counter's gaps must equal what the
 *                     host sent. Losing bytes is allowed; losing track is not.
 *                     -DUART_RX_BURST_N=<n> sets the byte count.
 *
 *   -DUART_RX_LINE    the host paces one line at a time (TB_PACE=line), so
 *                     nothing may be lost: uart_rx_getline must return the exact
 *                     text with rx_overruns()==0, and a following call with a
 *                     tick timeout must give up on time (mtime-measured).
 *
 * Print contract (firmware track): [TEST] <family> PASS|FAIL ... / SUMMARY n/m
 * / DONE, as in every other gate.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/uart_rx.h"
#include <omni/stdio.h>

#ifndef UART_RX_BURST_N
#define UART_RX_BURST_N 200u
#endif

#define MTIME_LO (*(volatile uint32_t *)0x0200BFF8u)

static uint32_t n_pass, n_fail;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

/* A deliberately dumb busy wait: this core has no wfi, and the point of the
 * burst test is to be busy *not* draining the FIFO. Returns its accumulator so
 * the loop cannot be optimised away (and so -Wall stays quiet). */
static uint32_t stall(uint32_t iters)
{
    volatile uint32_t x = 0;
    for (uint32_t i = 0; i < iters; i++) x += i;
    return x;
}

#ifdef UART_RX_BURST
static uint8_t buf[UART_RX_RING];

static void t_burst(void)
{
    unsigned got, lost = 0, i, last;
    char detail[96];

    uart_rx_init();
    (void)stall(150000u);           /* long enough for the FIFO to overrun */
    uart_rx_pump();                 /* one drain only: no draining while busy */
    (void)stall(150000u);           /* let anything still in flight arrive */
    uart_rx_pump();

    got = uart_rx_read(buf, UART_RX_RING);

    /* Reconstruct the loss from the counter's gaps. With a running counter the
     * delta between consecutive received bytes is 1 + (bytes dropped between
     * them), so the pattern *is* the sequence number here. */
    for (i = 1; i < got; i++)
        lost += (unsigned)((buf[i] - buf[i - 1] - 1) & 0xFF);
    last = got ? buf[got - 1] : 0;
    if (got) lost += (UART_RX_BURST_N - 1u) - last;

    snprintf(detail, sizeof detail, "consumed=%u lost=%u total=%u overruns=%u",
             got, lost, got + lost, uart_rx_overruns());
    report("burst", (got + lost == UART_RX_BURST_N) && uart_rx_overruns() > 0
                    && got < UART_RX_BURST_N, detail);
}
#endif

#ifdef UART_RX_LINE
static char line[64];

static void t_line(void)
{
    static const char want[] = "HELLO12345";
    int n = uart_rx_getline(line, sizeof line, 1000000u);
    char detail[96];

    snprintf(detail, sizeof detail, "len=%d text=%s overruns=%u",
             n, (n >= 0) ? line : "-", uart_rx_overruns());
    report("line", n == 10 && line[0] == want[0] && line[9] == want[9]
                   && uart_rx_overruns() == 0, detail);
}

static void t_timeout(void)
{
    uint32_t t0 = MTIME_LO, elapsed;
    int n;
    char detail[96];

    n = uart_rx_getline(line, sizeof line, 2000000u);       /* 40 ms at 50 MHz */
    elapsed = (uint32_t)(MTIME_LO - t0);
    snprintf(detail, sizeof detail, "ret=%d elapsed=%u asked=2000000", n, elapsed);
    /* timed out, and within 25% of what was asked for */
    report("timeout", n == UART_RX_TIMEOUT
                      && elapsed >= 1500000u && elapsed <= 2500000u, detail);
}
#endif

void app_main(void)
{
    uart_init();
    uart_rx_init();
    /* The harness (TB_START_MARK) holds its input back until this appears, so
     * nothing can land in the FIFO while we are not looking. */
    printf("[TEST] READY\r\n");
#ifdef UART_RX_BURST
    printf("[TEST] uart gate mode=burst n=%u\r\n", (unsigned)UART_RX_BURST_N);
    t_burst();
#elif defined(UART_RX_LINE)
    printf("[TEST] uart gate mode=line\r\n");
    t_line();
    t_timeout();
#else
    printf("[TEST] uart gate mode=none (build with -DUART_RX_BURST or -DUART_RX_LINE)\r\n");
    report("mode", 0, " no mode selected");
#endif
    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
