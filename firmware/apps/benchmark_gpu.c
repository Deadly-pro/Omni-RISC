// Omni-RISC APU — benchmark_gpu
//
// Coherent-APU demo (Phase F): the CPU writes two input vectors A and B into
// the GPU scratchpad through the shared host window (no copies), launches the
// vector_add kernel over MMIO, waits for the warp to halt, then reads the C
// results back over the same window and verifies C = A + B. Prints
// "GPU(OK) PASS\n" over UART if every lane matches.
#include "../drivers/uart.h"
#include "../drivers/gpu.h"

#define N 4

static uint32_t A[N] = { 10, 20, 30, 40 };
static uint32_t B[N] = {  1,  2,  3,  4 };

static void uart_uint(uint32_t v) {
    char b[12];
    unsigned n = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { b[n++] = '0' + (v % 10); v /= 10; }
    while (n) uart_putc(b[--n]);
}

static int gpu_check(void) {
    int ok = 1;
    for (unsigned i = 0; i < N; i++) {
        uint32_t c = gpu_read_word(i, 32);   // lane i, word 32 = C[i]
        uart_puts("C[");
        uart_uint(i);
        uart_puts("]=");
        uart_uint(c);
        uart_puts(" exp=");
        uart_uint(A[i] + B[i]);
        uart_puts("\n");
        ok &= (c == A[i] + B[i]);
    }
    return ok;
}

void app_main(void) {
    uart_puts("GPU dispatch\n");
    for (unsigned i = 0; i < N; i++) {
        gpu_write_word(i, 0,  A[i]);   // lane i, word 0  = A[i]
        gpu_write_word(i, 16, B[i]);   // lane i, word 16 = B[i]
    }
    gpu_launch(0);                 // warp0 starts at kernel pc 0
    gpu_wait_idle();               // poll active_warps until warp halts
    if (gpu_check())
        uart_puts("GPU(OK) PASS\n");
    else
        uart_puts("GPU(OK) FAIL\n");
}
