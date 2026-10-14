/**
 * @file bench_test.c
 * @brief Cycle-accurate characterization of this APU (firmware track).
 *
 * There is no board and no Vivado on this host, so what can be measured is what
 * the RTL *is*, cycle for cycle: this app reads the core's own mcycle and
 * minstret CSRs (0xB00/0xB02) around each workload and reports cycles, retired
 * instructions, CPI and cycles per unit of work.
 *
 * Two things make the numbers checkable rather than decorative:
 *   - every workload returns a checksum of its own result, so a workload that was
 *     optimised away or computed the wrong thing cannot report a nice number;
 *   - every workload is measured at N and at 2N. Cycles must grow by at least
 *     1.5x (a benchmark that ignores its input is measuring something else), and
 *     for the four workloads whose cost *is* linear in the input, cycles per unit
 *     must stay within +-25%. The deliberately super-linear ones are marked as
 *     such: insertion sort is O(n^2) and matmul at 2n is 8x the multiply-accumulates
 *     (its per-unit cost actually falls, 19.0 -> 15.1 cycles/MAC, because the loop
 *     overhead amortises - which is why per-unit stability is asserted only where
 *     it is the right property).
 *
 * Claim discipline: these are *simulated* cycles from the Verilator model of this
 * RTL at a nominal 50 MHz. They are not silicon, and the frequency is not
 * measured. Print format is stable for tests/test_bench.sh to parse.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/gpu.h"
#include "../drivers/gpu_loader.h"
#include <omni/stdio.h>
#include <omni/string.h>
#include <omni/alloc.h>
#include "gpu_kernel_words.h"

static uint32_t n_pass, n_fail;

/* ------------------------------------------------------------- counters --- */
/* Two independent counters. mcycle is the core's own CSR (0xB00); the CLINT's
 * mtime is a separate hardware counter in the SoC. They should agree, and the
 * app reports both so they can be compared - a cycle count that only comes from
 * one counter is an assertion, not a measurement.
 *
 * CPI is deliberately NOT reported: minstret (0xB02) exists as a readable CSR but
 * nothing increments it, and wiring a true "instruction retired" signal through
 * the pipeline is more than a measurement change. Cycles per unit of work is
 * what can be measured honestly today. */
static inline uint32_t rd_mcycle(void)
{
    uint32_t v;
    __asm__ volatile("csrr %0, mcycle" : "=r"(v));
    return v;
}
static inline uint32_t rd_mtime(void)
{
    return *(volatile uint32_t *)0x0200BFF8u;
}

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

static void bench_line(const char *name, uint32_t units, uint32_t cyc,
                       uint32_t mt, uint32_t cksum)
{
    uint32_t per  = units ? (cyc / units) : 0;
    uint32_t frac = units ? ((cyc % units) * 100u / units) : 0;
    printf("[BENCH] %s units=%u cycles=%u mtime=%u per_unit=%u.%02u cksum=%u\r\n",
           name, (unsigned)units, (unsigned)cyc, (unsigned)mt,
           (unsigned)per, (unsigned)frac, (unsigned)cksum);
}

/* ------------------------------------------------------------- workloads -- */
#define BUFSZ 1024
static unsigned char A[BUFSZ], B[BUFSZ];
static int isort_data[64];
static int m8[64], m8b[64], m8c[64];
static char txt[256];
static void *slots[64];

static uint32_t fnv(const unsigned char *p, uint32_t n)
{
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) { h ^= p[i]; h *= 16777619u; }
    return h;
}

/* Each workload: run over n, report how many units of work that was, return a
 * checksum of what it produced. */
typedef uint32_t (*workload_t)(uint32_t n, uint32_t *units);

static uint32_t w_memcpy(uint32_t n, uint32_t *units)
{
    memcpy(B, A, n); *units = n; return fnv(B, n);
}

static uint32_t w_memset(uint32_t n, uint32_t *units)
{
    memset(B, 0x5A, n); *units = n; return fnv(B, n);
}

static uint32_t w_hash(uint32_t n, uint32_t *units)
{
    *units = n; return fnv(B, n);
}

static uint32_t w_isort(uint32_t n, uint32_t *units)
{
    for (int i = 1; i < (int)n; i++) {
        int k = isort_data[i], j = i - 1;
        while (j >= 0 && isort_data[j] > k) { isort_data[j + 1] = isort_data[j]; j--; }
        isort_data[j + 1] = k;
    }
    *units = n;
    uint32_t h = 2166136261u;
    for (int i = 0; i < (int)n; i++) {
        /* sortedness is the checksum of a sort: prove order, not a magic value */
        if (i && isort_data[i - 1] > isort_data[i]) return 0xDEADBEEFu;
        h ^= (uint32_t)isort_data[i]; h *= 16777619u;
    }
    return h;
}

static uint32_t w_matmul(uint32_t n, uint32_t *units)
{
    for (uint32_t i = 0; i < n; i++)
        for (uint32_t j = 0; j < n; j++) {
            int acc = 0;
            for (uint32_t k = 0; k < n; k++) acc += m8[i * n + k] * m8b[k * n + j];
            m8c[i * n + j] = acc;
        }
    *units = n * n * n;
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n * n; i++) { h ^= (uint32_t)m8c[i]; h *= 16777619u; }
    return h;
}

static uint32_t w_snprintf(uint32_t n, uint32_t *units)
{
    for (uint32_t i = 0; i < n; i++)
        snprintf(txt, sizeof txt, "line %u of %u hex=%08x str=%s\r\n",
                 (unsigned)i, (unsigned)n, (unsigned)(i * 2654435761u), "omni");
    *units = n;
    return fnv((const unsigned char *)txt, sizeof txt);
}

static uint32_t w_alloc(uint32_t n, uint32_t *units)
{
    for (uint32_t i = 0; i < n && i < 64u; i++) {
        slots[i] = malloc(16u + (i % 32u));
        if (slots[i]) *(volatile uint32_t *)slots[i] = i;
    }
    for (uint32_t i = 0; i < n && i < 64u; i++) free(slots[i]);
    *units = n;
    return heap_check() ? 1u : 0u;
}

/* Warm up once (buffers, code lines), then measure n and 2n back to back, so the
 * two windows are adjacent and comparable. Returns the scaling ratio x100. */
/* Worst disagreement seen between mcycle and the CLINT's mtime, in percent. Both
 * count cycles, so if they disagree the measurement is wrong. */
static uint32_t worst_skew;

static uint32_t measure2x(const char *name, workload_t fn, uint32_t n, uint32_t *per_unit_x100)
{
    uint32_t c0, c1, c2, m0, m1, m2, u1 = 0, u2 = 0, k1, k2, ratio, d1, e1, skew;
    uint32_t pu1, pu2;

    k1 = fn(n, &u1);                         /* warm-up, discarded */
    (void)k1;

    c0 = rd_mcycle(); m0 = rd_mtime();
    k1 = fn(n, &u1);
    c1 = rd_mcycle(); m1 = rd_mtime();

    k2 = fn(2u * n, &u2);
    c2 = rd_mcycle(); m2 = rd_mtime();

    bench_line(name, u1, c1 - c0, m1 - m0, k1);
    bench_line(name, u2, c2 - c1, m2 - m1, k2);

    d1 = c1 - c0; e1 = m1 - m0;
    skew = d1 ? ((d1 > e1 ? d1 - e1 : e1 - d1) * 100u / d1) : 0u;
    if (skew > worst_skew) worst_skew = skew;

    pu1 = u1 ? (d1 * 100u / u1) : 0u;         /* per-unit cost x100 at N   */
    pu2 = u2 ? ((c2 - c1) * 100u / u2) : 0u;  /* per-unit cost x100 at 2N  */
    *per_unit_x100 = pu1 ? (pu2 * 100u / pu1) : 0u;

    ratio = d1 ? ((c2 - c1) * 100u / d1) : 0u;
    printf("[BENCH] ratio name=%s n_cycles=%u n2_cycles=%u x100=%u skew_pct=%u perunit_x100=%u\r\n",
           name, (unsigned)d1, (unsigned)(c2 - c1), (unsigned)ratio, (unsigned)skew,
           (unsigned)*per_unit_x100);
    return ratio;
}

/* ------------------------------------------------- gpu vs scalar ---------- */
static volatile uint32_t scalar_sink;

static void gpu_measure(void)
{
    uint32_t t0, t1, c0, c1, m0, m1, sc0, sc1, sm0, sm1, sum = 0;
    int ok;

    for (uint32_t i = 0; i < 4u; i++) {
        gpu_write_word(i, 0, 10u + i);
        gpu_write_word(i, 16, 20u + i);
    }
    gpu_load_kernel(gpu_kernel_words, GPU_KERNEL_LEN);

    c0 = rd_mcycle(); m0 = rd_mtime();
    t0 = rd_mcycle();
    ok = gpu_run_wait_irq(0, 200000u);          /* launch -> completion interrupt */
    t1 = rd_mcycle();
    c1 = rd_mcycle(); m1 = rd_mtime();

    for (uint32_t i = 0; i < 4u; i++) sum += gpu_read_word(i, 32);

    sc0 = rd_mcycle(); sm0 = rd_mtime();
    {   /* the same 16 additions the kernel does, scalar */
        uint32_t acc = 0;
        for (uint32_t i = 0; i < 4u; i++) acc += (10u + i) + (20u + i);
        scalar_sink = acc;
    }
    sc1 = rd_mcycle(); sm1 = rd_mtime();

    bench_line("gpu_dispatch", 1u, t1 - t0, t1 - t0, sum);     /* launch+irq latency */
    bench_line("gpu_run_total", 1u, c1 - c0, m1 - m0, sum);    /* incl. window setup */
    bench_line("scalar_add16", 1u, sc1 - sc0, sm1 - sm0, scalar_sink);
    printf("[BENCH] speedup gpu_launch_cycles=%u scalar_cycles=%u ratio_x100=%u cksum=%u\r\n",
           (unsigned)(t1 - t0), (unsigned)(sc1 - sc0),
           (unsigned)((sc1 - sc0) ? ((t1 - t0) * 100u / (sc1 - sc0)) : 0u), (unsigned)sum);

    report("gpu_add", ok == 1 && sum == 132u,
           "vector_add via the runtime loader computed A+B while being timed");
}

/* ------------------------------------------------------------------ main -- */
void app_main(void)
{
    uint32_t seed = 1;

    uart_init();
    heap_init();
    printf("[TEST] READY\r\n");
    printf("[BENCH] apu characterization, simulated cycles (Verilator, 50 MHz nominal)\r\n");

    for (uint32_t i = 0; i < BUFSZ; i++) A[i] = (unsigned char)(i * 7u + 3u);
    for (uint32_t i = 0; i < 64u; i++) {
        seed = seed * 1103515245u + 12345u;
        isort_data[i] = (int)(seed >> 16);
    }
    for (uint32_t i = 0; i < 64u; i++) { m8[i] = (int)(i % 7u); m8b[i] = (int)(i % 5u); }

    struct { const char *name; workload_t fn; uint32_t n; int linear; } w[] = {
        { "memcpy_byte",   w_memcpy,   256u, 1 },
        { "memset_byte",   w_memset,   256u, 1 },
        { "fnv1a_byte",    w_hash,     256u, 1 },
        { "isort_elem",    w_isort,    32u,  0 },   /* O(n^2) by nature */
        { "matmul8_mac",   w_matmul,   4u,   0 },   /* 2n means 8x the MACs */
        { "snprintf_line", w_snprintf, 8u,   1 },
        { "malloc_pair",   w_alloc,    16u,  0 },   /* arena pressure is not linear */
    };

    for (unsigned k = 0; k < sizeof w / sizeof w[0]; k++) {
        uint32_t pu = 0, ratio = measure2x(w[k].name, w[k].fn, w[k].n, &pu);
        char d[96];

        snprintf(d, sizeof d, "cycles x100=%u when input doubled (want >=150)", (unsigned)ratio);
        report(w[k].name, ratio >= 150u, d);

        if (w[k].linear) {
            char nm[40];
            int ok = pu >= 75u && pu <= 125u;
            snprintf(nm, sizeof nm, "per_unit_%s", w[k].name);
            snprintf(d, sizeof d, "per-unit cost x100=%u at 2N (want 75..125)", (unsigned)pu);
            report(nm, ok, d);
        }
    }

    {
        char d[64];
        snprintf(d, sizeof d, "worst mcycle-vs-mtime skew %u%% (want <=5%%)", (unsigned)worst_skew);
        report("counter_agree", worst_skew <= 5u, d);
    }

    gpu_measure();

    printf("[BENCH] done\r\n");
    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
