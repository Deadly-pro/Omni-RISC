/**
 * @file mlp_test.c
 * @brief A real workload on the GPU: a 2-layer MLP, kernel images reloaded
 *        between launches (firmware track, P2).
 *
 * Until F8 a kernel could only be flashed at synthesis time, so a multi-kernel
 * computation like this was impossible: there was one image in the GPU and it
 * never changed. Here the firmware pushes matmul into the GPU's instruction
 * memory, runs it, pushes relu over the top, runs that, then goes back to matmul
 * for the second layer - four launches, four loads, two different kernels, with
 * the CPU orchestrating between them over the shared-memory window.
 *
 * The workload: y = relu(W2 . relu(W1 . x)), integer 2x2 matrices, computed on
 * four samples at once (one sample per SIMT lane, each lane with its own
 * scratchpad bank). The CPU then computes the same thing scalar, in C, from the
 * same tables, and the app requires the two to agree per lane - so the GPU
 * result is checked, not admired.
 *
 * Kernel layouts are the kernels' own, from their headers:
 *   matmul.S  A @ 0..3, B @ 4..7, C @ 8..11 (2x2 row-major, per lane)
 *   relu.S    X @ 0..3, Y @ 16..19
 * Only the first column of B is meaningful: B = [[v0,0],[v1,0]] makes C[0] and
 * C[2] the two components of the matrix-vector product.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/gpu.h"
#include "../drivers/gpu_loader.h"
#include <omni/stdio.h>
#include "gpu_mlp_kernels.h"

#define LANES 4
#define W_MATMUL   KERN_MATMUL_LEN
#define W_RELU     KERN_RELU_LEN

static uint32_t n_pass, n_fail;
static unsigned loads;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

/* Deterministic weights and inputs: one row of four per lane (2x2, row-major). */
static const int32_t W1[LANES][4] = {
    {  1,  2,  3,  4 },
    {  2, -1,  1,  3 },
    { -1,  2,  2,  1 },
    {  3,  1, -2,  2 },
};
static const int32_t X[LANES][2] = {
    { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 },
};
static const int32_t W2[LANES][4] = {
    {  1,  1,  1, -1 },
    {  2,  1, -1,  1 },
    {  1, -2,  2,  1 },
    { -1,  1,  1,  2 },
};

static int32_t relu_ref(int32_t v) { return v > 0 ? v : 0; }

/* What the GPU is supposed to produce, computed the boring way. */
static void mlp_ref(unsigned lane, int32_t *y0, int32_t *y1)
{
    int32_t a = (int32_t)(uint32_t)X[lane][0], b = (int32_t)(uint32_t)X[lane][1];
    int32_t h0 = relu_ref(W1[lane][0] * a + W1[lane][1] * b);
    int32_t h1 = relu_ref(W1[lane][2] * a + W1[lane][3] * b);
    *y0 = relu_ref(W2[lane][0] * h0 + W2[lane][1] * h1);
    *y1 = relu_ref(W2[lane][2] * h0 + W2[lane][3] * h1);
}

/* ------------------------------------------------------ gpu plumbing ----- */
static void stage_a_b(unsigned lane, const int32_t *a, const int32_t *b2)
{
    for (int k = 0; k < 4; k++) gpu_write_word(lane, (uint32_t)k, (uint32_t)a[k]);
    gpu_write_word(lane, 4, (uint32_t)b2[0]);
    gpu_write_word(lane, 5, 0);
    gpu_write_word(lane, 6, (uint32_t)b2[1]);
    gpu_write_word(lane, 7, 0);
}

static void copy_within(unsigned lane, uint32_t from, uint32_t to, uint32_t n)
{
    for (uint32_t k = 0; k < n; k++)
        gpu_write_word(lane, to + k, gpu_read_word(lane, from + k));
}

static int launch(const uint16_t *words, uint32_t len, const char *what)
{
    gpu_load_kernel(words, len);
    loads++;
    if (gpu_imem_addr() != len) {
        printf("[MLP] load failed: wanted %u words, pointer says %u\r\n",
               (unsigned)len, (unsigned)gpu_imem_addr());
        return 0;
    }
    if (!gpu_run_wait_irq(0, 200000u)) {
        printf("[MLP] %s did not signal completion\r\n", what);
        return 0;
    }
    return 1;
}

/* ------------------------------------------------------------------ run -- */
void app_main(void)
{
    int32_t got_y0[LANES], got_y1[LANES], want_y0[LANES], want_y1[LANES];
    uint32_t gpu_cyc, sc_cyc, c0, c1;
    int ok = 1;

    uart_init();
    printf("[TEST] READY\r\n");
    printf("[MLP] 2-layer MLP, %d samples in parallel, kernel reloaded per layer\r\n",
           (int)LANES);
    printf("[MLP] kernel images: matmul=%u words, relu=%u words\r\n",
           (unsigned)W_MATMUL, (unsigned)W_RELU);

    c0 = (uint32_t)*(volatile uint32_t *)0x0200BFF8u;   /* CLINT mtime */

    /* layer 1: matmul(W1, x) */
    for (unsigned i = 0; i < LANES; i++) stage_a_b(i, W1[i], X[i]);
    ok = launch(kern_matmul_words, W_MATMUL, "matmul(layer1)") && ok;

    /* relu on the matmul output: C @ 8..11 -> X @ 0..3, then relu writes Y @ 16 */
    for (unsigned i = 0; i < LANES; i++) copy_within(i, 8, 0, 4);
    ok = launch(kern_relu_words, W_RELU, "relu(layer1)") && ok;

    /* layer 2: matmul(W2, h), reading h back out of Y[0] and Y[2] */
    for (unsigned i = 0; i < LANES; i++) {
        int32_t h[2];
        h[0] = (int32_t)gpu_read_word(i, 16);
        h[1] = (int32_t)gpu_read_word(i, 18);
        stage_a_b(i, W2[i], h);
    }
    ok = launch(kern_matmul_words, W_MATMUL, "matmul(layer2)") && ok;

    for (unsigned i = 0; i < LANES; i++) copy_within(i, 8, 0, 4);
    ok = launch(kern_relu_words, W_RELU, "relu(layer2)") && ok;

    for (unsigned i = 0; i < LANES; i++) {
        got_y0[i] = (int32_t)gpu_read_word(i, 16);
        got_y1[i] = (int32_t)gpu_read_word(i, 18);
    }
    c1 = (uint32_t)*(volatile uint32_t *)0x0200BFF8u;
    gpu_cyc = c1 - c0;

    /* the same thing scalar */
    {
        uint32_t s0 = (uint32_t)*(volatile uint32_t *)0x0200BFF8u;
        for (unsigned i = 0; i < LANES; i++) mlp_ref(i, &want_y0[i], &want_y1[i]);
        sc_cyc = (uint32_t)*(volatile uint32_t *)0x0200BFF8u - s0;
    }

    for (unsigned i = 0; i < LANES; i++) {
        char d[96];
        int lane_ok = (got_y0[i] == want_y0[i]) && (got_y1[i] == want_y1[i]);
        snprintf(d, sizeof d, "lane%u y=(%d,%d) scalar=(%d,%d)",
                 i, (int)got_y0[i], (int)got_y1[i], (int)want_y0[i], (int)want_y1[i]);
        {
            char nm[16];
            snprintf(nm, sizeof nm, "lane%u", i);
            report(nm, lane_ok, d);
        }
        ok = ok && lane_ok;
    }

    {
        char d[96];
        snprintf(d, sizeof d, "%u loads: matmul, relu, matmul, relu (%u and %u words)",
                 loads, (unsigned)W_MATMUL, (unsigned)W_RELU);
        report("reload", ok && loads == 4u && W_MATMUL != W_RELU, d);
    }

    printf("[MLP] gpu_cycles=%u scalar_cycles=%u ramp_ratio_x100=%u\r\n",
           (unsigned)gpu_cyc, (unsigned)sc_cyc,
           (unsigned)(sc_cyc ? (gpu_cyc * 100u / sc_cyc) : 0u));
    report("cost", gpu_cyc > 0 && sc_cyc > 0, "both paths were timed (simulated cycles)");

    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
