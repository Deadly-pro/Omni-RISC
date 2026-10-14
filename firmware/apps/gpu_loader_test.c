/**
 * @file gpu_loader_test.c
 * @brief F8 gate app: runtime GPU kernel loading (firmware track).
 *
 * The same app is built twice, and that pair is the whole point:
 *
 *   -DGPU_LOAD=1   the vector_add words are pushed over the load port, then the
 *                  kernel is launched and the completion interrupt is awaited
 *   -DGPU_LOAD=0   nothing is pushed
 *
 * With nothing in the GPU's instruction memory there is no kernel to run to a
 * halt, so the completion interrupt never arrives and the result never appears.
 * The gate asserts both halves, which is what makes "the kernel really was
 * loaded at run time" a measurement instead of an assumption.
 *
 * Inputs are staged in warp0's shared-memory window the same way the Phase-F
 * demo and the shell do it: lane i word 0 = A[i], word 16 = B[i]. The kernel's
 * own header states where it puts the result - A @ 0, B @ 16, **C @ 32** - which
 * is where this app reads it back from (reading word 0 instead returns A, since
 * nothing overwrote it).
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/gpu.h"
#include "../drivers/gpu_loader.h"
#include <omni/stdio.h>
#include "gpu_kernel_words.h"

#ifndef GPU_LOAD
#define GPU_LOAD 1
#endif

#define LANES 8

static uint32_t n_pass, n_fail;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

void app_main(void)
{
    uint32_t c[LANES];
    uint32_t sum = 0, expect = 0, pushed;
    int waited;

    uart_init();
    printf("[TEST] READY\r\n");
    printf("[TEST] gpu_load gate load=%d words=%d\r\n", (int)GPU_LOAD, (int)GPU_KERNEL_LEN);

    /* Stage A[i] and B[i] in the shared window (warp0 scratchpad). */
    for (int i = 0; i < 4; i++) {
        gpu_write_word((uint32_t)i, 0, (uint32_t)(10 + i));
        gpu_write_word((uint32_t)i, 16, (uint32_t)(20 + i));
        expect += (uint32_t)(10 + i) + (uint32_t)(20 + i);
    }

#if GPU_LOAD
    gpu_load_kernel(gpu_kernel_words, GPU_KERNEL_LEN);
#endif
    pushed = gpu_imem_addr();
    printf("[TEST] gpu_load imem_addr=%u\r\n", (unsigned)pushed);

    waited = gpu_run_wait_irq(0, 200000u);      /* 200 ms is minutes of GPU work */
    printf("[TEST] gpu_load waited=%d handler=%d irq=%u\r\n",
           waited, gpu_irq_handler_ran(), gpu_irq_count());

    for (int i = 0; i < 4; i++) sum += gpu_read_word((uint32_t)i, 32);
    printf("[TEST] gpu_load lanes=%u,%u,%u,%u sum=%u expected=%u\r\n",
           c[0] = gpu_read_word(0, 32), c[1] = gpu_read_word(1, 32),
           c[2] = gpu_read_word(2, 32), c[3] = gpu_read_word(3, 32),
           (unsigned)sum, (unsigned)expect);

#if GPU_LOAD
    report("imem", pushed == (uint32_t)GPU_KERNEL_LEN,
           "load pointer advanced exactly one word per pushed instruction");
    report("completion", waited == 1 && gpu_irq_handler_ran() != 0,
           "completion interrupt arrived (waited on msip, not on STATUS)");
    report("result", sum == expect,
           "vector_add computed A+B in the scratchpad the CPU staged");
#else
    report("imem", pushed == 0u, "no kernel pushed: the load pointer is untouched");
    report("completion", waited == 0,
           "no kernel in imem, so no warp ran to completion and no interrupt came");
    report("result", sum != expect,
           "and with no kernel the result is not there - loading is what computes it");
#endif

    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
