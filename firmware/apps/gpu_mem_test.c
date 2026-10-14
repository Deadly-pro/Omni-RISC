/**
 * @file gpu_mem_test.c
 * @brief M2: a GPU kernel reading and writing the CPU's own memory (unified memory).
 *
 * This is the thing the MMIO staging window could not do. The data is an ordinary
 * array in DMEM - the same memory the CPU's loads and stores use - and the kernel
 * reaches it through LDG/STG with the GPU's master port. There is no
 * gpu_write_word, no host window, no copy: the CPU writes buf[0] and buf[1] with
 * plain assignments, the kernel adds them, and the CPU reads the answer with a
 * plain read of buf[2].
 *
 * The GPU finds the array through GPU_MEM_BASE (0x4000_202C), which the CPU sets
 * once, so the kernel itself only carries small offsets.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include "../drivers/gpu.h"
#include "../drivers/gpu_loader.h"
#include <omni/stdio.h>
#include "gpu_mem_kernel.h"

#define GPU_MEM_BASE_REG (0x4000202Cu)

static uint32_t n_pass, n_fail;

static void report(const char *fam, int ok, const char *detail)
{
    printf("[TEST] %s %s %s\r\n", fam, ok ? "PASS" : "FAIL", detail ? detail : "");
    if (ok) n_pass++; else n_fail++;
}

/* Ordinary memory. volatile + its address taken keeps it in DMEM rather than in
 * registers, which is exactly what the GPU needs to be able to see it. The
 * buffer sits inside a wider window so that a store landing at the wrong offset
 * can be found rather than guessed at. */
#define BUF_OFF 16
static volatile uint32_t win[64];
#define buf (win + BUF_OFF)

void app_main(void)
{
    uint32_t base;
    int waited;

    uart_init();
    printf("[TEST] READY\r\n");

    buf[0] = 111u;      /* plain C stores, no MMIO */
    buf[1] = 222u;
    buf[2] = 0xDEADBEEFu;
    base = (uint32_t)(uintptr_t)&buf[0];

    printf("[TEST] gpu_mem base=0x%08x a=%u b=%u\r\n",
           (unsigned)base, (unsigned)buf[0], (unsigned)buf[1]);

    *(volatile uint32_t *)GPU_MEM_BASE_REG = base;   /* the GPU's window onto it */
    printf("[TEST] gpu_mem reg_base=0x%08x\r\n",
           (unsigned)*(volatile uint32_t *)GPU_MEM_BASE_REG);

    gpu_load_kernel(kern_mem_add_words, KERN_MEM_ADD_LEN);
    printf("[TEST] gpu_mem imem_addr=%u want=%u\r\n",
           (unsigned)gpu_imem_addr(), (unsigned)KERN_MEM_ADD_LEN);
    waited = gpu_run_wait_irq(0, 200000u);

    printf("[TEST] gpu_mem done=%d c=%u irq=%u\r\n",
           waited, (unsigned)buf[2], gpu_irq_count());

    /* Where did the store actually land? Scan the window for the expected sum
     * (never dereference address 0: gcc turns that constant into an ebreak). */
    {
        int found = -1, i;
        for (i = 0; i < 64; i++) if (win[i] == 333u) { found = i; break; }
        printf("[TEST] gpu_mem found333=%d want=%d abs8=%u\r\n",
               found, BUF_OFF + 2, (unsigned)*(volatile uint32_t *)(uintptr_t)8u);
    }
    /* GPU debug: did the handshake start, and where did it get to? */
    printf("[TEST] gpu_mem dbg_starts=%u dbg_state=0x%08x dbg_last=0x%08x\r\n",
           (unsigned)*(volatile uint32_t *)0x40002030u,
           (unsigned)*(volatile uint32_t *)0x40002034u,
           (unsigned)*(volatile uint32_t *)0x40002038u);

    report("gpu_mem", waited == 1 && buf[2] == 333u,
           "kernel added the CPU's own array words and stored back into it");

    /* And the other direction, to show it is genuinely two-way: a value written
     * by the CPU after the launch is visible to a second kernel. */
    buf[0] = 5u; buf[1] = 37u;
    gpu_load_kernel(kern_mem_add_words, KERN_MEM_ADD_LEN);
    waited = gpu_run_wait_irq(0, 200000u);
    report("gpu_mem_again", waited == 1 && buf[2] == 42u,
           "a second launch saw the CPU's newer values through the same memory");

    printf("[TEST] SUMMARY %u/%u\r\n", n_pass, n_pass + n_fail);
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
