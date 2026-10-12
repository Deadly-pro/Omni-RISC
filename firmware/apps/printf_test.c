/**
 * @file printf_test.c
 * @brief F2 gate app: snprintf/vsnprintf vectors, plus printf/vprintf streaming.
 *
 * This file is part of the GATE, not of your implementation. The expectations
 * are not mine: tests/gen_printf_vectors.py emits the case table, and a host C
 * program runs the SAME cases through the HOST C LIBRARY's snprintf and writes
 * firmware/apps/printf_expected.h. glibc is the reference, so "what printf
 * should do" is settled by the C library, not by anyone's reading of a man page.
 *
 *     tests/test_printf.sh        generate -> host reference -> run this -> assert
 *
 * The case table (firmware/apps/printf_vectors.h) is shared with the host
 * reference, and the buffer/guard convention it documents is implemented
 * identically on both sides.
 *
 * Two families:
 *   snprintf   every case, compared against the host expectation (ret, guard,
 *              and the first min(n, PF_OUT) buffer bytes) - self-checked, so a
 *              failure names the case and the byte.
 *   stream     printf() and then vprintf() run the same short formats between
 *              markers; the gate diffs each block against the host's expected
 *              bytes, which also proves the two entry points agree.
 *
 * Discipline: the app never formats its own output with the library under test.
 * Reporting goes through uart_puts (a driver), and the app is built with
 * -fno-builtin (see the Makefile) so gcc cannot fold the literal-format calls
 * into inline copies. Weak, deliberately wrong stubs make the gate runnable
 * before you have written anything.
 */
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include "../drivers/uart.h"
#include "printf_vectors.h"
#include "printf_expected.h"
#include <omni/stdio.h>

/* ------------------------------------------------------------- stubs ------
 * Wrong on purpose, weak so your strong definitions in firmware/lib override
 * them. Do not edit these to make anything pass. */
#define GATE_STUB __attribute__((weak))

GATE_STUB int vsnprintf(char *b, size_t n, const char *f, va_list ap)
{ (void)b; (void)n; (void)f; (void)ap; return 0; }
GATE_STUB int snprintf(char *b, size_t n, const char *f, ...)
{ (void)b; (void)n; (void)f; return 0; }
GATE_STUB int vprintf(const char *f, va_list ap) { (void)f; (void)ap; return 0; }
GATE_STUB int printf(const char *f, ...) { (void)f; return 0; }

/* ------------------------------------------------------------- reporting -- */
static uint32_t n_pass, n_fail;

static void put_u32(uint32_t v)
{
    char b[11];
    int i = 10;
    b[10] = 0;
    do { b[--i] = (char)('0' + (v % 10u)); v /= 10u; } while (v);
    uart_puts(&b[i]);
}

static void put_hex2(unsigned v)
{
    const char *d = "0123456789abcdef";
    char b[4];
    b[0] = d[(v >> 4) & 0xF];
    b[1] = d[v & 0xF];
    b[2] = ' ';
    b[3] = 0;
    uart_puts(b);
}

static const char *fam_name;
static uint32_t fam_pass, fam_fail;

/* Transcript budget: real baud, so counts are always exact and only this many
 * failure detail lines per family reach the wire. */
#define MAX_DETAIL 3

static void fam_begin(const char *name) { fam_name = name; fam_pass = 0; fam_fail = 0; }

static void fam_end(void)
{
    uart_puts("[TEST] ");
    uart_puts(fam_name);
    uart_puts(" ");
    put_u32(fam_pass);
    uart_puts("/");
    put_u32(fam_pass + fam_fail);
    uart_puts(fam_fail ? " FAIL\r\n" : " PASS\r\n");
    if (fam_fail > MAX_DETAIL) {
        uart_puts("[TEST] ");
        uart_puts(fam_name);
        uart_puts(" (+");
        put_u32(fam_fail - MAX_DETAIL);
        uart_puts(" more failures not printed)\r\n");
    }
}

static unsigned detail_shown;

static void detail_begin(unsigned idx)
{
    if (detail_shown >= MAX_DETAIL) return;
    detail_shown++;
    uart_puts("[TEST] ");
    uart_puts(fam_name);
    uart_puts("[");
    put_u32(idx);
    uart_puts("] FAIL ");
}

/* -------------------------------------------------------- the one call ---- */
/* Identical dispatch to the host reference (printf_ref_host.c): the shape
 * constants come from the shared header. */
static unsigned char buf[PF_BUF];

static void buf_fill(void)
{
    for (unsigned i = 0; i < PF_BUF; i++) buf[i] = 0xA5;
}

static int pf_call(const pf_case_t *c)
{
    switch (c->shape) {
    case PF_I1:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a);
    case PF_U1:   return snprintf((char *)buf, c->n, c->fmt, (unsigned)c->a);
    case PF_IU:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a, (unsigned)c->b);
    case PF_S1:   return snprintf((char *)buf, c->n, c->fmt, c->s);
    case PF_C1:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a);
    case PF_P1:   return snprintf((char *)buf, c->n, c->fmt, (void *)(uintptr_t)c->a);
    case PF_I2:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a, (int)c->b);
    default:      return snprintf((char *)buf, c->n, c->fmt);
    }
}

static unsigned written_of(int ret, unsigned n)
{
    if (n == 0 || ret < 0) return 0;
    return ((unsigned)ret + 1u <= n) ? (unsigned)ret + 1u : n;
}

static void t_snprintf(void)
{
    fam_begin("snprintf");
    detail_shown = 0;
    for (unsigned i = 0; i < N_PF_CASES; i++) {
        const pf_case_t *c = &pf_cases[i];
        const pf_expect_t *e = &pf_expect[i];
        unsigned n = c->n;
        unsigned shown = (n < PF_OUT) ? n : PF_OUT;

        buf_fill();
        int ret = pf_call(c);

        unsigned written = written_of(ret, n);
        int guard = 1;
        for (unsigned k = written; k < PF_BUF; k++)
            if (buf[k] != 0xA5) { guard = 0; break; }

        int ok = (ret == e->ret) && (guard == (int)e->guard_ok);
        int bad_byte = -1;
        for (unsigned k = 0; k < shown && ok; k++)
            if (buf[k] != e->out[k]) { ok = 0; bad_byte = (int)k; }

        if (ok) { fam_pass++; n_pass++; continue; }
        fam_fail++; n_fail++;

        if (detail_shown >= MAX_DETAIL) continue;
        detail_begin(i);
        uart_puts("fmt=");
        uart_puts(c->fmt);
        uart_puts(" n=");
        put_u32(n);
        if (ret != e->ret) {
            uart_puts(" ret want=");
            put_u32((uint32_t)e->ret);
            uart_puts(" got=");
            put_u32((uint32_t)ret);
        }
        if (guard != (int)e->guard_ok) {
            uart_puts(" guard want=");
            put_u32((uint32_t)e->guard_ok);
            uart_puts(" got=");
            put_u32((uint32_t)guard);
        }
        if (bad_byte >= 0) {
            uart_puts(" byte[");
            put_u32((uint32_t)bad_byte);
            uart_puts("] want=0x");
            put_hex2((unsigned)e->out[bad_byte]);
            uart_puts("got=0x");
            put_hex2((unsigned)buf[bad_byte]);
        }
        uart_puts(" |");
        for (unsigned k = 0; k < shown; k++) {
            uart_puts(" ");
            put_hex2(buf[k]);
        }
        uart_puts("\r\n");
    }
    fam_end();
}

/* ------------------------------------------------------------- stream ----- */
/* printf and vprintf run the same formats; the gate diffs both blocks against
 * the same host-produced expectation, which also cross-checks the two paths. */
static int vwrap(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}

static void t_stream(void)
{
    uart_puts("[PRINTF-STREAM-BEGIN]\r\n");
    for (unsigned i = 0; i < N_PF_STREAM; i++) {
        const pf_stream_t *c = &pf_stream[i];
        switch (c->shape) {
        case PF_I1: printf(c->fmt, (int)c->a); break;
        case PF_U1: printf(c->fmt, (unsigned)c->a); break;
        case PF_IU: printf(c->fmt, (int)c->a, (unsigned)c->b); break;
        case PF_S1: printf(c->fmt, c->s); break;
        case PF_C1: printf(c->fmt, (int)c->a); break;
        case PF_P1: printf(c->fmt, (void *)(uintptr_t)c->a); break;
        case PF_I2: printf(c->fmt, (int)c->a, (int)c->b); break;
        default:    printf(c->fmt); break;
        }
    }
    uart_puts("[PRINTF-STREAM-END]\r\n");

    uart_puts("[VPRINTF-STREAM-BEGIN]\r\n");
    for (unsigned i = 0; i < N_PF_STREAM; i++) {
        const pf_stream_t *c = &pf_stream[i];
        switch (c->shape) {
        case PF_I1: vwrap(c->fmt, (int)c->a); break;
        case PF_U1: vwrap(c->fmt, (unsigned)c->a); break;
        case PF_IU: vwrap(c->fmt, (int)c->a, (unsigned)c->b); break;
        case PF_S1: vwrap(c->fmt, c->s); break;
        case PF_C1: vwrap(c->fmt, (int)c->a); break;
        case PF_P1: vwrap(c->fmt, (void *)(uintptr_t)c->a); break;
        case PF_I2: vwrap(c->fmt, (int)c->a, (int)c->b); break;
        default:    vwrap(c->fmt); break;
        }
    }
    uart_puts("[VPRINTF-STREAM-END]\r\n");
}

/* ----------------------------------------------------------------- main -- */
void app_main(void)
{
    uart_init();

    uart_puts("[TEST] printf gate cases=");
    put_u32(N_PF_CASES);
    uart_puts(" stream=");
    put_u32(N_PF_STREAM);
    uart_puts("\r\n");

    t_snprintf();
    t_stream();

    uart_puts("[TEST] SUMMARY ");
    put_u32(n_pass);
    uart_puts("/");
    put_u32(n_pass + n_fail);
    uart_puts("\r\n");
    uart_puts("[TEST] DONE\r\n");

    for (;;) { }        /* the harness ends the simulation on the marker */
}
