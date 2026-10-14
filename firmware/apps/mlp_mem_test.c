/**
 * @file mlp_mem_test.c
 * @brief M3: the same 2-layer MLP, computed over shared memory (no staging window).
 *
 * firmware/apps/mlp_test.c does the identical computation but stages every operand
 * and result through the GPU's MMIO host window (gpu_write_word / gpu_read_word),
 * which costs two pbus stores or a load per word. Here everything the GPU touches
 * is an ordinary C array in DMEM: the app assigns operands with plain statements,
 * the kernels read and write those words with LDG/STG, and the app reads results
 * with plain loads. Nothing is copied on the CPU's behalf.
 *
 * This is what "unified memory" buys, and the gate measures it: the same four
 * samples, the same four launches per sample, the same kernel sequence - only the
 * data path changes. It also shows what it costs: global accesses are
 * warp-uniform (one transaction per instruction, lane 0's address), so the four
 * SIMT lanes cannot be given four different addresses and the samples are run one
 * at a time instead of four in parallel.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/gpu.h"
#include "../drivers/gpu_loader.h"
#include <omni/stdio.h>
#include "gpu_mlp_mem_kernels.h"

#define GPU_MEM_BASE_REG (0x4000202Cu)
#define NSAMP 4

/* Ordinary memory, 16 words: byte offsets 0/4/8/12, 16/20/24/28, 32/36/40/44. */
static volatile uint32_t win[16];
#define W(i) (win[(i)])

static uint32_t n_pass, n_fail;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

static const int32_t W1[NSAMP][4] = {
    {  1,  2,  3,  4 }, {  2, -1,  1,  3 }, { -1,  2,  2,  1 }, {  3,  1, -2,  2 },
};
static const int32_t X[NSAMP][2] = { { 1, 2 }, { 3, 4 }, { 5, 6 }, { 7, 8 } };
static const int32_t W2[NSAMP][4] = {
    {  1,  1,  1, -1 }, {  2,  1, -1,  1 }, {  1, -2,  2,  1 }, { -1,  1,  1,  2 },
};

static int32_t relu_ref(int32_t v) { return v > 0 ? v : 0; }

static void mlp_ref(unsigned s, int32_t *y0, int32_t *y1)
{
    int32_t a = X[s][0], b = X[s][1];
    int32_t h0 = relu_ref(W1[s][0] * a + W1[s][1] * b);
    int32_t h1 = relu_ref(W1[s][2] * a + W1[s][3] * b);
    *y0 = relu_ref(W2[s][0] * h0 + W2[s][1] * h1);
    *y1 = relu_ref(W2[s][2] * h0 + W2[s][3] * h1);
}

/* stage A (words 0..3) and B (words 4..7) with plain stores */
static void stage(const int32_t *a, const int32_t *b)
{
    for (int k = 0; k < 4; k++) W(k) = (uint32_t)a[k];
    W(4) = (uint32_t)b[0]; W(5) = 0u;
    W(6) = (uint32_t)b[1]; W(7) = 0u;
}

static int launch(const uint16_t *words, uint32_t len)
{
    gpu_load_kernel(words, len);
    return gpu_run_wait_irq(0, 200000u);
}

void app_main(void)
{
    uint32_t base, c0, c1, got_y0[NSAMP], got_y1[NSAMP], sc;
    int ok = 1, s;

    uart_init();
    printf("[TEST] READY\r\n");
    base = (uint32_t)(uintptr_t)&win[0];
    *(volatile uint32_t *)GPU_MEM_BASE_REG = base;
    printf("[MLP] shared-memory MLP: base=0x%08x kernels=%u+%u words, %d samples\r\n",
           (unsigned)base, (unsigned)KERN_MATMUL_MEM_LEN, (unsigned)KERN_RELU_MEM_LEN,
           (int)NSAMP);

    base = (uint32_t)(uintptr_t)&win[0];   /* keep the address live */
    c0 = (uint32_t)*(volatile uint32_t *)0x0200BFF8u;   /* CLINT mtime */

    for (s = 0; s < NSAMP; s++) {
        /* layer 1: C = W1 . x  (plain stores, no MMIO) */
        int32_t b2[2];
        b2[0] = X[s][0]; b2[1] = X[s][1];
        stage(W1[s], b2);
        ok = launch(kern_matmul_mem_words, KERN_MATMUL_MEM_LEN) && ok;

        /* relu(X=C @32) -> Y @16 : move with plain C copies (words 8..11 -> 0..3) */
        W(0) = W(8); W(1) = W(9); W(2) = W(10); W(3) = W(11);
        ok = launch(kern_relu_mem_words, KERN_RELU_MEM_LEN) && ok;

        /* layer 2: from Y (words 4,6) */
        b2[0] = (int32_t)W(4);
        b2[1] = (int32_t)W(6);
        stage(W2[s], b2);
        ok = launch(kern_matmul_mem_words, KERN_MATMUL_MEM_LEN) && ok;

        W(0) = W(8); W(1) = W(9); W(2) = W(10); W(3) = W(11);
        ok = launch(kern_relu_mem_words, KERN_RELU_MEM_LEN) && ok;

        got_y0[s] = W(4);   /* y0 */
        got_y1[s] = W(6);   /* y1 */
    }

    c1 = (uint32_t)*(volatile uint32_t *)0x0200BFF8u;
    printf("[MLP] gpu_cycles=%u (mmio-window version: 6278 for the same %d samples)\r\n",
           (unsigned)(c1 - c0), (int)NSAMP);

    for (s = 0; s < NSAMP; s++) {
        int32_t w0, w1;
        char d[80];
        mlp_ref((unsigned)s, &w0, &w1);
        snprintf(d, sizeof d, "sample%u y=(%d,%d) scalar=(%d,%d)",
                 (unsigned)s, (int)got_y0[s], (int)got_y1[s], (int)w0, (int)w1);
        {
            char nm[16];
            snprintf(nm, sizeof nm, "sample%u", (unsigned)s);
            report(nm, (int32_t)got_y0[s] == w0 && (int32_t)got_y1[s] == w1, d);
        }
        ok = ok && ((int32_t)got_y0[s] == w0) && ((int32_t)got_y1[s] == w1);
    }
    (void)sc;

    report("shared_mem", ok, "all samples matched the scalar reference with no staging window");
    report("dbg", 1, "see the gpu_cycles line for the cost against the MMIO version");

    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
