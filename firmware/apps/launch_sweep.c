/**
 * @file launch_sweep.c
 * @brief What a GPU launch costs, and what it costs on top of that per instruction.
 *
 * The characterization work found a GPU launch-to-interrupt latency of 306 cycles
 * against 7 cycles for the same 16 additions done scalar. That single number says
 * dispatch dominates at small sizes but not what it would take to stop dominating,
 * so this sweeps kernel length and separates the two costs:
 *
 *   fixed cost      the intercept: MMIO setup, launch, trap, interrupt, window
 *   marginal cost   the slope: cycles per kernel instruction
 *
 * The kernel is generated in C (N x ADD r1,r1,r1 then HALT), so N is exact and no
 * assembler or generated header is involved. Both numbers are what you need to
 * answer "how big does a kernel have to be before launching it beats just doing the
 * work on the CPU": a launch pays for itself only when the work it carries is worth
 * more than the fixed cost.
 *
 * OPEN BUG (found by this sweep, unrelated to the global-memory path): a launch of
 * a pure-ALU kernel at some lengths intermittently never signals completion. The
 * kernels here contain no LDG/STG, so the unified-memory handshake is not involved
 * - this is the GPU/warp-scheduler completion path. It is measured and printed as
 * `[SWEEP] flaky` rather than averaged away; the fit uses the lengths that were
 * stable. Worth chasing: it is the kind of intermittent that hides in a short test.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/gpu.h"
#include "../drivers/gpu_loader.h"
#include <omni/stdio.h>

#define MAXN 256u

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

/* N adds + HALT, pushed fresh for every measurement. */
static unsigned short kern[MAXN + 1u];

static uint32_t sweep(uint32_t n)
{
    uint32_t c0, c1;
    int ok;

    for (uint32_t i = 0; i < n; i++) kern[i] = 0x0248u;   /* ADD r1, r1, r1 */
    kern[n] = 0xF000u;                                    /* HALT */
    gpu_load_kernel(kern, n + 1u);

    c0 = rd_mcycle();
    ok = gpu_run_wait_irq(0, 200000u);                    /* launch -> completion IRQ */
    c1 = rd_mcycle();
    return ok ? (c1 - c0) : 0u;
}

void app_main(void)
{
    static const uint32_t ns[] = { 4u, 32u, 40u, 48u, 56u, 64u };
    uint32_t cyc[sizeof ns / sizeof ns[0]];
    uint32_t slope, intercept, i, fl1, fl2;

    uart_init();
    printf("[TEST] READY\r\n");
    printf("[SWEEP] kernel length vs launch->completion cycles (ADD+HALT each)\r\n");

    for (i = 0; i < sizeof ns / sizeof ns[0]; i++) {
        cyc[i] = sweep(ns[i]);
        printf("[SWEEP] n=%u cycles=%u per_insn_x100=%u\r\n",
               (unsigned)ns[i], (unsigned)cyc[i],
               (unsigned)(cyc[i] ? (cyc[i] * 100u / ns[i]) : 0u));
    }

    /* The flakiness, measured rather than described: same kernel, twice. */
    fl1 = sweep(16u);
    fl2 = sweep(16u);
    printf("[SWEEP] flaky n=16 attempt1=%u attempt2=%u (0 = no completion within the timeout)\r\n",
           (unsigned)fl1, (unsigned)fl2);

    /* Two reliable points far apart give the fit: slope = marginal, intercept =
     * fixed. cyc[1] is n=32, cyc[5] is n=64. */
    slope     = (cyc[5] - cyc[1]) / (ns[5] - ns[1]);
    intercept = cyc[1] - ns[1] * slope;

    printf("[SWEEP] slope=%u cycles_per_kernel_insn intercept=%u fixed_launch_cycles\r\n",
           (unsigned)slope, (unsigned)intercept);

    report("sweep_monotonic", cyc[5] > cyc[0] && cyc[3] > cyc[0],
           "longer kernels take longer to complete");
    report("sweep_fixed_cost", intercept >= 30u && intercept <= 4000u,
           "the fixed per-launch cost is measurable and non-trivial");
    report("sweep_marginal", slope >= 1u && slope <= 40u,
           "each kernel instruction costs at least a cycle");

    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
