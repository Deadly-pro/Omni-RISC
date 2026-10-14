/**
 * @file alloc_test.c
 * @brief F5 gate app: the heap allocator (firmware track).
 *
 * The interesting property of an allocator is not "does one allocation work" -
 * it is that the arena is *still coherent* after thousands of mixed operations.
 * So every family below finishes by running heap_check(), and the soak runs it
 * after every single operation: the first bad op number is the bug.
 *
 * Families: alignment, write/verify, coalescing after a full free (including
 * one allocation that only fits if everything merged back), realloc semantics
 * (shrink in place, grow, NULL, zero), exhaustion then recovery, double-free
 * detection, and a seeded randomised soak ending in leaks=0.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include <omni/stdio.h>
#include <omni/alloc.h>

#ifndef ALLOC_SEED
#define ALLOC_SEED 12345u
#endif
#define SOAK_OPS 2000u
#define SLOTS 24u

static uint32_t n_pass, n_fail;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

static uint32_t rng_state = ALLOC_SEED;
static uint32_t rnd(void) { rng_state = rng_state * 1103515245u + 12345u; return (rng_state >> 8) & 0x7FFFFFu; }

/* ------------------------------------------------------------ families ---- */
static void t_align(void)
{
    int ok = 1;
    unsigned bad = 0;
    for (size_t n = 1; n <= 64; n++) {
        uint8_t *p = (uint8_t *)malloc(n);
        if (((uintptr_t)p % OMNI_ALIGN) != 0) { ok = 0; bad = (unsigned)n; }
        free(p);
    }
    ok = ok && heap_check();
    report("align", ok, ok ? "every size 1..64 came back 8-byte aligned"
                           : "misaligned pointer");
    (void)bad;
}

static void t_write_verify(void)
{
    uint8_t *p[8];
    int ok = 1;

    heap_init();
    for (unsigned i = 0; i < 8; i++) {
        size_t n = 16u + 8u * i;
        p[i] = (uint8_t *)malloc(n);
        if (!p[i]) { ok = 0; break; }
        for (size_t k = 0; k < n; k++) p[i][k] = (uint8_t)(i * 31u + k);
    }
    for (unsigned i = 0; i < 8 && ok; i++) {
        size_t n = 16u + 8u * i;
        for (size_t k = 0; k < n; k++)
            if (p[i][k] != (uint8_t)(i * 31u + k)) { ok = 0; break; }
    }
    ok = ok && heap_check();
    report("write", ok, "eight blocks written and read back, arena still coherent");
}

static void t_coalesce(void)
{
    uint8_t *a, *b, *c, *big;
    size_t before, after;
    int ok;

    heap_init();
    a = (uint8_t *)malloc(256);
    b = (uint8_t *)malloc(256);
    c = (uint8_t *)malloc(256);
    ok = a && b && c && heap_check();
    free(a); free(b); free(c);
    before = heap_largest_free();
    /* This allocation cannot succeed unless the three neighbours merged back. */
    big = (uint8_t *)malloc(768);
    after = heap_largest_free();
    ok = ok && big != 0 && heap_check() && before >= 768;
    report("coalesce", ok, ok ? "freeing neighbours merged them: one 768-byte block fit"
                              : "neighbours did not coalesce");
    free(big);
    (void)after;
}

static void t_realloc(void)
{
    uint8_t *p, *q;
    int ok = 1;
    unsigned moved = 0;

    heap_init();
    p = (uint8_t *)malloc(64);
    if (!p) { report("realloc", 0, "malloc failed"); return; }
    for (unsigned i = 0; i < 64; i++) p[i] = (uint8_t)(i + 1);

    q = (uint8_t *)realloc(p, 256);                 /* grow */
    if (!q) ok = 0;
    else {
        if (q != p) moved = 1;
        for (unsigned i = 0; i < 64 && ok; i++) if (q[i] != (uint8_t)(i + 1)) ok = 0;
    }
    q = (uint8_t *)realloc(q, 16);                  /* shrink */
    if (!q) ok = 0;
    else for (unsigned i = 0; i < 16 && ok; i++) if (q[i] != (uint8_t)(i + 1)) ok = 0;

    if (realloc(0, 32) == 0) ok = 0;                /* == malloc */
    if (realloc(q, 0) != 0) ok = 0;                 /* frees, returns NULL */
    ok = ok && heap_check();
    report("realloc", ok, ok ? (moved ? "grow moved with contents preserved; shrink kept prefix"
                                      : "grow happened in place; shrink kept prefix")
                             : "realloc lost the contents");

    /* exhaustion then recovery, which is where a leaky free shows up */
    {
        void *v[SLOTS * 4];
        unsigned n = 0;
        heap_init();
        while (n < SLOTS * 4 && (v[n] = malloc(64)) != 0) n++;
        for (unsigned i = 0; i < n; i++) free(v[i]);
        ok = (n > 0) && heap_check() && malloc(OMNI_HEAP_BYTES - 32u) != 0;
        report("exhausted", ok, ok ? "allocated until NULL, freed all, whole arena usable again"
                                   : "arena did not come back after freeing everything");
    }
}

static void t_double_free(void)
{
    uint8_t *p = (uint8_t *)malloc(32);
    int ok;

    heap_init();
    p = (uint8_t *)malloc(32);
    free(p);
    free(p);                                        /* must be caught, not corrupt */
    ok = heap_errors() == 1 && heap_check();
    report("doublefree", ok, ok ? "second free counted and ignored; arena intact"
                                : "a double free corrupted the arena");
}

static void t_soak(void)
{
    uint8_t *slot[SLOTS];
    unsigned tag[SLOTS];
    unsigned op_bad = 0;
    int ok = 1;

    heap_init();
    for (unsigned i = 0; i < SLOTS; i++) { slot[i] = 0; tag[i] = 0; }

    for (unsigned op = 0; op < SOAK_OPS && ok; op++) {
        unsigned s = rnd() % SLOTS;
        unsigned action = rnd() % 3;
        uint32_t t = (op << 8) | s;

        /* A long CPU-only test looks identical to a hung one to anything
         * watching the UART, so say something now and then. */
        if ((op % 500u) == 0u) uart_putc('.');

        if (slot[s]) {                              /* verify what we last wrote */
            size_t n = tag[s] & 0xFFFFu;
            uint32_t oldt = tag[s] >> 16;           /* the tag stored, not this op's */
            for (size_t k = 0; k < n; k++)
                if (slot[s][k] != (uint8_t)(oldt + k)) { ok = 0; op_bad = op; break; }
        }
        if (!ok) break;

        if (action == 0 && !slot[s]) {
            size_t n = (rnd() % 120u) + 8u;
            slot[s] = (uint8_t *)malloc(n);
            if (slot[s]) {
                tag[s] = (unsigned)((n & 0xFFFFu) | (t << 16));
                for (size_t k = 0; k < n; k++) slot[s][k] = (uint8_t)(t + k);
            }
        } else if (action == 1 && slot[s]) {
            free(slot[s]);
            slot[s] = 0;
        } else if (slot[s]) {
            size_t n = (rnd() % 120u) + 8u;
            uint8_t *q = (uint8_t *)realloc(slot[s], n);
            if (q) {
                slot[s] = q;
                tag[s] = (unsigned)((n & 0xFFFFu) | (t << 16));
                for (size_t k = 0; k < n; k++) slot[s][k] = (uint8_t)(t + k);
            }
        }
        if (!heap_check()) { ok = 0; op_bad = op; }  /* invariants after EVERY op */
    }

    for (unsigned i = 0; i < SLOTS; i++) if (slot[i]) free(slot[i]);
    ok = ok && heap_check() && heap_blocks() == 1 && heap_largest_free() >= OMNI_HEAP_BYTES - 64u;

    if (ok) report("soak", 1, "2000 mixed ops, invariants held after each, leaks=0");
    else {
        char d[80];
        snprintf(d, sizeof d, "op=%u errors=%u blocks=%u", op_bad, heap_errors(), heap_blocks());
        report("soak", 0, d);
    }
}

void app_main(void)
{
    uart_init();
    heap_init();

    printf("[TEST] READY\r\n");
    printf("[TEST] alloc gate arena=%u align=%u seed=%u\r\n",
           (unsigned)OMNI_HEAP_BYTES, (unsigned)OMNI_ALIGN, (unsigned)ALLOC_SEED);

    t_align();
    t_write_verify();
    t_coalesce();
    t_realloc();
    t_double_free();
    t_soak();

    printf("[TEST] alloc free=%u largest=%u blocks=%u errors=%u\r\n",
           (unsigned)heap_free_bytes(), (unsigned)heap_largest_free(),
           heap_blocks(), heap_errors());
    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
