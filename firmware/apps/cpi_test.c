/**
 * @file cpi_test.c
 * @brief Cycles per instruction, measured where the instruction count is exact.
 *
 * CPI is the headline microarchitecture number and this core cannot produce it from
 * its CSRs: `minstret` is readable but nothing increments it, because the pipeline
 * has no "an instruction retired" signal to count (see RESULTS.md). Adding one means
 * a valid bit through all four pipeline registers, which touches the hold/flush
 * timing that has already produced two bugs here.
 *
 * So this measures CPI the other way round: cycles from `mcycle`, instruction count
 * known *by construction* rather than counted. Two shapes, both exact:
 *
 *   taken-branch loop   2 instructions per iteration (addi + bnez), the branch taken
 *                       every time - which is the worst case for a core with no
 *                       branch predictor, and the number says what that costs
 *   straight line       64 dependent addi, no branches at all: the pipeline's
 *                       steady-state cost
 *
 * Both are timed over enough iterations that the fixed prologue is noise. The gate
 * re-derives CPI from the app's own printed cycles and instruction counts, so the
 * arithmetic is checked rather than trusted.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include <omni/stdio.h>

#define ITERS   4096u
#define STRAIGHT 64u

static uint32_t n_pass, n_fail;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

static inline uint32_t rd_mcycle(void)
{
    uint32_t v;
    __asm__ volatile("csrr %0, mcycle" : "=r"(v));
    return v;
}

/* 2 instructions per iteration, branch taken every time. */
static uint32_t taken_branch_loop(uint32_t n)
{
    uint32_t i = n;
    __asm__ volatile(
        "1:\n\t"
        "addi %0, %0, -1\n\t"
        "bnez %0, 1b\n\t"
        : "+r"(i) : : "memory");
    return i;
}

/* 64 dependent addi in a row, no control flow at all. */
static uint32_t straight_line(void)
{
    uint32_t v = 1;
    __asm__ volatile(
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t" "addi %0, %0, 1\n\t"
        : "+r"(v) : : "memory");
    return v;
}

/* cycles x100 / instructions, printed without floating point (no libgcc here). */
static void cpi_line(const char *name, uint32_t cycles, uint32_t insns)
{
    uint32_t cpi100 = insns ? (cycles * 100u / insns) : 0u;
    printf("[CPI] %s cycles=%u instructions=%u cpi=%u.%02u\r\n",
           name, (unsigned)cycles, (unsigned)insns,
           (unsigned)(cpi100 / 100u), (unsigned)(cpi100 % 100u));
}

void app_main(void)
{
    uint32_t c0, c1, cyc;
    uint32_t cpi_branch100, cpi_straight100;

    uart_init();
    printf("[TEST] READY\r\n");
    printf("[CPI] %u-iteration taken-branch loop, %u straight-line addi\r\n",
           (unsigned)ITERS, (unsigned)STRAIGHT);

    /* warm up, then measure: the prologue and the call are outside both windows */
    (void)taken_branch_loop(16u);
    c0 = rd_mcycle();
    (void)taken_branch_loop(ITERS);
    c1 = rd_mcycle();
    cyc = c1 - c0;
    cpi_line("taken_branch", cyc, 2u * ITERS);
    cpi_branch100 = (2u * ITERS) ? (cyc * 100u / (2u * ITERS)) : 0u;

    (void)straight_line();
    c0 = rd_mcycle();
    (void)straight_line();
    c1 = rd_mcycle();
    cyc = c1 - c0;
    cpi_line("straight_line", cyc, STRAIGHT);
    cpi_straight100 = (cyc * 100u) / STRAIGHT;

    /* A 5-stage in-order core with no branch predictor, no cache and full
     * forwarding: straight-line code should sit at or just above 1.0 CPI, and a
     * taken branch should be visibly worse. Bands are deliberately loose - this is
     * asserting the shape of the machine, not a cycle-exact model. */
    report("cpi_straight", cpi_straight100 >= 90u && cpi_straight100 <= 200u,
           "straight-line code runs at about one instruction per cycle");
    report("cpi_branch", cpi_branch100 > cpi_straight100,
           "a taken branch costs more per instruction than straight-line code");
    report("cpi_reported", cpi_branch100 > 0u && cpi_straight100 > 0u,
           "both CPIs reported for the gate to re-derive");

    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
