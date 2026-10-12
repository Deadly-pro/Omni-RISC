/**
 * @file libc_test.c
 * @brief F1 gate app: memory + string library vectors (firmware track).
 *
 * This file is part of the GATE, not of your implementation. It exercises the
 * sources in firmware/lib against the tables tests/gen_libc_vectors.py
 * generates:
 *
 *     tests/test_libc.sh      memory + string half  (11 functions)
 *     tests/test_convert.sh   the same app built with APP_DEFS=-DWITH_CONV,
 *                             adding the numeric-conversion half
 *
 * Print contract (firmware track):
 *     [TEST] <family> <pass>/<total> PASS      one line per family when green
 *     [TEST] <family>[<idx>] FAIL want=.. got=..   a detail line per bad case
 *     [TEST] SUMMARY <pass>/<total>
 *     [TEST] DONE                <- ends the simulation; the harness watches for it
 *
 * The family lines exist because the UART really runs at 115200 baud: every
 * byte costs ~4,340 simulated cycles, so a hundred per-case lines would make
 * each gate run take minutes. A green run is a dozen short lines; a red one
 * leads with the exact cases that failed.
 *
 * Discipline this file follows on purpose: it never uses the library under test
 * for its own scaffolding. Reporting goes through uart_puts (drivers, not the
 * library), and every buffer is built and checked with the local helpers below,
 * with -fno-builtin on this file (see the Makefile) so gcc cannot quietly
 * rewrite those helper loops back into memcpy/memset/memcmp/strlen calls. Your
 * implementation is only ever called for the operation under test.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "libc_vectors.h"
#include <omni/string.h>
#ifdef WITH_CONV
#include "conv_vectors.h"
#endif

/* ------------------------------------------------------------- stubs ------
 * Deliberately WRONG, WEAK definitions, so this app links and runs before you
 * have written anything: you get per-case PASS/FAIL from your first function
 * instead of "undefined reference to `memcpy'". Your real implementations in
 * firmware/lib/ are strong symbols and override these the moment they exist.
 *
 * They live here, in the gate app - not in lib/ - so no other app can pick them
 * up by accident.
 *
 * Do not edit these to make anything pass; the point is that they fail.
 */
#define GATE_STUB __attribute__((weak))

GATE_STUB void  *memcpy (void *d, const void *s, size_t n) { (void)d;(void)s;(void)n; return d; }
GATE_STUB void  *memmove(void *d, const void *s, size_t n) { (void)d;(void)s;(void)n; return d; }
GATE_STUB void  *memset (void *s, int c, size_t n)         { (void)s;(void)c;(void)n; return s; }
GATE_STUB int    memcmp (const void *a, const void *b, size_t n) { (void)a;(void)b;(void)n; return 0; }
GATE_STUB size_t strlen (const char *s)                    { (void)s; return 0; }
GATE_STUB char  *strcpy (char *d, const char *s)           { (void)d;(void)s; return d; }
GATE_STUB char  *strncpy(char *d, const char *s, size_t n) { (void)d;(void)s;(void)n; return d; }
GATE_STUB int    strcmp (const char *a, const char *b)     { (void)a;(void)b; return 0; }
GATE_STUB int    strncmp(const char *a, const char *b, size_t n) { (void)a;(void)b;(void)n; return 0; }
GATE_STUB char  *strchr (const char *s, int c)             { (void)s;(void)c; return 0; }
GATE_STUB char  *strrchr(const char *s, int c)             { (void)s;(void)c; return 0; }
#ifdef WITH_CONV
GATE_STUB unsigned long strtoul(const char *s, char **e, int b)
{ (void)b; if (e) *e = (char *)s; return 0; }
GATE_STUB long strtol(const char *s, char **e, int b)
{ (void)b; if (e) *e = (char *)s; return 0; }
GATE_STUB int  omni_conv_errno(void) { return 0; }
GATE_STUB char *itoa(int v, char *b)     { (void)v; if (b) b[0] = 0; return b; }
GATE_STUB char *utoa_hex(unsigned long v, char *b) { (void)v; if (b) b[0] = 0; return b; }
#endif

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

static const char *fam_name;
static uint32_t fam_pass, fam_fail;

/* Transcript budget: the UART is real 115200 baud, so every printed byte costs
 * ~4,340 simulated cycles. Counts are ALWAYS exact; only this many failure
 * detail lines per family are printed. */
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

/* One check: counts always, a transcript line only when it fails. */
static void report(unsigned idx, int ok, uint32_t want, uint32_t got)
{
    if (ok) { fam_pass++; n_pass++; return; }
    fam_fail++; n_fail++;
    if (fam_fail > MAX_DETAIL) return;      /* counts above are still exact */
    uart_puts("[TEST] ");
    uart_puts(fam_name);
    uart_puts("[");
    put_u32(idx);
    uart_puts("] FAIL want=");
    put_u32(want);
    uart_puts(" got=");
    put_u32(got);
    uart_puts("\r\n");
}

/* sign encoded for the transcript: 0 = zero, 1 = negative, 2 = positive */
#define SENC(s) ((uint32_t)((s) < 0 ? 1u : ((s) > 0 ? 2u : 0u)))

/* ------------------------------------------------- local scaffolding ----- */
/* Deliberately NOT the library under test. Byte-wise, obviously correct. */
static void lfill(unsigned char *p, unsigned int n, unsigned char v)
{ for (unsigned int i = 0; i < n; i++) p[i] = v; }

static unsigned int lseq(const unsigned char *a, const unsigned char *b, unsigned int n)
{ for (unsigned int i = 0; i < n; i++) if (a[i] != b[i]) return i + 1; return 0; }

static unsigned int lsmis(const unsigned char *a, const unsigned char *b)
{ for (unsigned int i = 0; i < N_BUF; i++) if (a[i] != b[i]) return i; return N_BUF; }

static unsigned int lslen(const char *s)
{ unsigned int n = 0; while (s[n]) n++; return n; }

static int lseq_s(const char *a, const char *b)
{ unsigned int i = 0; while (a[i] && a[i] == b[i]) i++; return a[i] == b[i]; }

/* sign of the comparison, C semantics (unsigned char) */
static int lscmp(const char *a, const char *b)
{
    unsigned int i = 0;
    while (a[i] && (unsigned char)a[i] == (unsigned char)b[i]) i++;
    unsigned char ca = (unsigned char)a[i], cb = (unsigned char)b[i];
    return (ca == cb) ? 0 : (ca < cb ? -1 : 1);
}

static unsigned char A[N_BUF], B[N_BUF], C[N_BUF], EXP[N_BUF], TMP[N_BUF];
static char SA[N_BUF + 8];

/* ------------------------------------------------------------- families -- */
static void t_memcpy(void)
{
    fam_begin("memcpy");
    for (unsigned i = 0; i < N_MEM_CASES; i++) {
        const mem_case_t *c = &mem_cases[i];
        for (unsigned k = 0; k < N_BUF; k++) A[k] = (k < c->len) ? c->src[k] : 0xA5;
        lfill(C, N_BUF, 0xC3);
        for (unsigned k = 0; k < N_BUF; k++) EXP[k] = (k < c->n) ? A[k] : 0xC3;

        void *ret = memcpy(C, A, c->n);
        unsigned bad = lsmis(C, EXP);
        int ok = (ret == (void *)C) && (bad == N_BUF);
        report(i, ok, c->n, ok ? 0u : bad);
    }
    fam_end();
}

static void t_memmove(void)
{
    fam_begin("memmove");
    for (unsigned i = 0; i < N_MEM_CASES; i++) {
        const mem_case_t *c = &mem_cases[i];
        for (unsigned k = 0; k < N_BUF; k++) A[k] = (k < c->len) ? c->src[k] : 0xA5;
        for (unsigned k = 0; k < N_BUF; k++) TMP[k] = A[k];      /* snapshot */
        for (unsigned k = 0; k < N_BUF; k++) EXP[k] = TMP[k];
        for (unsigned k = 0; k < c->n; k++) EXP[c->dst_off + k] = TMP[k];

        unsigned char *dst = A + c->dst_off;
        void *ret = memmove(dst, A, c->n);
        unsigned bad = lsmis(A, EXP);
        int ok = (ret == (void *)dst) && (bad == N_BUF);
        report(i, ok, c->n, ok ? 0u : bad);
    }
    fam_end();
}

static void t_memset(void)
{
    fam_begin("memset");
    for (unsigned i = 0; i < N_MEM_CASES; i++) {
        const mem_case_t *c = &mem_cases[i];
        lfill(C, N_BUF, 0xC3);
        for (unsigned k = 0; k < N_BUF; k++) EXP[k] = (k < c->n) ? c->fill : 0xC3;

        void *ret = memset(C, c->fill, c->n);
        unsigned bad = lsmis(C, EXP);
        int ok = (ret == (void *)C) && (bad == N_BUF);
        report(i, ok, c->fill, ok ? 0u : bad);
    }
    fam_end();
}

static void t_memcmp(void)
{
    fam_begin("memcmp");
    for (unsigned i = 0; i < N_MCMP_CASES; i++) {
        const mcmp_case_t *c = &mcmp_cases[i];
        for (unsigned k = 0; k < N_BUF; k++) { A[k] = c->a[k]; B[k] = c->b[k]; }
        int r = memcmp(A, B, c->n);
        int s = (r > 0) - (r < 0);
        report(i, s == c->sign, SENC(c->sign), SENC(s));
    }
    fam_end();
}

static void t_strlen(void)
{
    fam_begin("strlen");
    for (unsigned i = 0; i < N_LEN_CASES; i++) {
        const len_case_t *c = &len_cases[i];
        unsigned got = (unsigned)strlen(c->s);
        report(i, got == c->len, c->len, got);
    }
    fam_end();
}

static void t_strcpy(void)
{
    fam_begin("strcpy");
    for (unsigned i = 0; i < N_CPY_CASES; i++) {
        const cpy_case_t *c = &cpy_cases[i];
        unsigned len = lslen(c->src);
        lfill((unsigned char *)SA, sizeof SA, 0xC3);

        char *ret = strcpy(SA, c->src);
        int ok = (ret == SA) && lseq_s(SA, c->src) && ((unsigned char)SA[len + 1] == 0xC3);
        report(i, ok, len, lslen(SA));
    }
    fam_end();
}

static void t_strncpy(void)
{
    fam_begin("strncpy");
    for (unsigned i = 0; i < N_CPY_CASES; i++) {
        const cpy_case_t *c = &cpy_cases[i];
        unsigned len = lslen(c->src);
        lfill((unsigned char *)SA, sizeof SA, 0xC3);
        /* expected: n bytes, NUL-padded, and NO terminator when truncated */
        for (unsigned k = 0; k < c->n; k++)
            EXP[k] = (k < len) ? (unsigned char)c->src[k] : 0;

        char *ret = strncpy(SA, c->src, c->n);
        unsigned bad = lseq((unsigned char *)SA, EXP, c->n);      /* 0 = equal */
        int guard = ((unsigned char)SA[c->n] == 0xC3);
        int ok = (ret == SA) && (bad == 0) && guard;
        report(i, ok, c->n, ok ? 0u : (bad ? bad : 0xFFu));
    }
    fam_end();
}

static void t_strcmp(void)
{
    fam_begin("strcmp");
    for (unsigned i = 0; i < N_CMP_CASES; i++) {
        const cmp_case_t *c = &cmp_cases[i];
        int want = lscmp(c->a, c->b);            /* full strings */
        int got  = strcmp(c->a, c->b);
        int s = (got > 0) - (got < 0);
        report(i, s == want, SENC(want), SENC(s));
    }
    fam_end();
}

static void t_strncmp(void)
{
    fam_begin("strncmp");
    for (unsigned i = 0; i < N_CMP_CASES; i++) {
        const cmp_case_t *c = &cmp_cases[i];
        int got = strncmp(c->a, c->b, c->n);     /* expectation is the prefix */
        int s = (got > 0) - (got < 0);
        report(i, s == c->sign, SENC(c->sign), SENC(s));
    }
    fam_end();
}

static void t_strchr(int reverse)
{
    fam_begin(reverse ? "strrchr" : "strchr");
    for (unsigned i = 0; i < N_CHR_CASES; i++) {
        const chr_case_t *c = &chr_cases[i];
        int want = reverse ? c->last : c->first;
        const char *p = reverse ? strrchr(c->s, c->ch) : strchr(c->s, c->ch);
        const char *exp = (want < 0) ? NULL : (c->s + want);
        unsigned got = (p == NULL) ? 0xFFFFFFFFu : (unsigned)(p - c->s);
        unsigned w   = (want < 0) ? 0xFFFFFFFFu : (unsigned)want;
        report(i, p == exp, w, got);
    }
    fam_end();
}

#ifdef WITH_CONV
/* The conversion checks report one case per function, but name all three
 * aspects (value / endptr / ERANGE) when they fail: "want=5 got=0" alone would
 * not say whether the digits, the endptr or the saturation went wrong. */
static void report_conv(unsigned idx, int val_ok, int end_ok, int er_ok,
                        unsigned long want_v, unsigned long got_v,
                        unsigned want_e, unsigned got_e)
{
    if (val_ok && end_ok && er_ok) { fam_pass++; n_pass++; return; }
    fam_fail++; n_fail++;
    if (fam_fail > MAX_DETAIL) return;      /* counts above are still exact */

    uart_puts("[TEST] ");
    uart_puts(fam_name);
    uart_puts("[");
    put_u32(idx);
    uart_puts("] FAIL");
    if (!val_ok) {
        uart_puts(" value want="); put_u32((uint32_t)want_v);
        uart_puts(" got=");        put_u32((uint32_t)got_v);
    }
    if (!end_ok) {
        uart_puts(" endptr want="); put_u32(want_e);
        uart_puts(" got=");         put_u32(got_e);
    }
    if (!er_ok) uart_puts(" ERANGE mismatch");
    uart_puts("\r\n");
}

static void t_strtoul(void)
{
    fam_begin("strtoul");
    for (unsigned i = 0; i < N_CONV_CASES; i++) {
        const conv_case_t *c = &conv_cases[i];
        /* poison the endptr: an implementation that never writes it must not be
         * able to look correct */
        char *end = (char *)(uintptr_t)0x1234u;
        unsigned long got = strtoul(c->s, &end, c->base);
        int er = omni_conv_errno();
        unsigned off = (unsigned)((uintptr_t)end - (uintptr_t)c->s);
        report_conv(i, got == c->expect_u, off == c->end_off,
                    (er != 0) == (c->erange_u != 0),
                    c->expect_u, got, c->end_off, off);
    }
    fam_end();
}

static void t_strtol(void)
{
    fam_begin("strtol");
    for (unsigned i = 0; i < N_CONV_CASES; i++) {
        const conv_case_t *c = &conv_cases[i];
        char *end = (char *)(uintptr_t)0x1234u;
        long got = strtol(c->s, &end, c->base);
        int er = omni_conv_errno();
        unsigned off = (unsigned)((uintptr_t)end - (uintptr_t)c->s);
        report_conv(i, got == c->expect_l, off == c->end_off,
                    (er != 0) == (c->erange_l != 0),
                    (unsigned long)c->expect_l, (unsigned long)got, c->end_off, off);
    }
    fam_end();
}

static void t_itoa(void)
{
    fam_begin("itoa");
    for (unsigned i = 0; i < N_ITOA_CASES; i++) {
        const itoa_case_t *c = &itoa_cases[i];
        char b[16];
        lfill((unsigned char *)b, sizeof b, '~');
        char *ret = itoa(c->v, b);
        /* the documented minimum buffer is 12 bytes: byte 12 must be untouched */
        int ok = (ret == b) && lseq_s(b, c->dec) && (b[12] == '~');
        report(i, ok, (uint32_t)lslen(c->dec), (uint32_t)lslen(b));
    }
    fam_end();
}

static void t_utoa_hex(void)
{
    fam_begin("utoa_hex");
    for (unsigned i = 0; i < N_HEX_CASES; i++) {
        const hex_case_t *c = &hex_cases[i];
        char b[16];
        lfill((unsigned char *)b, sizeof b, '~');
        char *ret = utoa_hex(c->v, b);
        /* documented minimum buffer is 9 bytes: byte 9 must be untouched */
        int ok = (ret == b) && lseq_s(b, c->hex) && (b[9] == '~');
        report(i, ok, (uint32_t)lslen(c->hex), (uint32_t)lslen(b));
    }
    fam_end();
}
#endif /* WITH_CONV */

/* ----------------------------------------------------------------- main -- */
void app_main(void)
{
    uart_init();

    uart_puts("[TEST] libc gate seed=");
    put_u32(VEC_SEED);
    uart_puts(" cases=");
    put_u32(N_LIBC_CASES);
#ifdef WITH_CONV
    uart_puts("+");
    put_u32(N_CONV_TOTAL);
#endif
    uart_puts("\r\n");

    t_memcpy();
    t_memmove();
    t_memset();
    t_memcmp();
    t_strlen();
    t_strcpy();
    t_strncpy();
    t_strcmp();
    t_strncmp();
    t_strchr(0);
    t_strchr(1);
#ifdef WITH_CONV
    t_strtoul();
    t_strtol();
    t_itoa();
    t_utoa_hex();
#endif

    uart_puts("[TEST] SUMMARY ");
    put_u32(n_pass);
    uart_puts("/");
    put_u32(n_pass + n_fail);
    uart_puts("\r\n");
    uart_puts("[TEST] DONE\r\n");

    for (;;) { }        /* the harness ends the simulation on the marker */
}
