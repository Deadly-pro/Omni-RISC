// Omni-RISC APU — benchmark_gpu
//
// End-to-end APU demo: the CPU dispatches a kernel to the GPU over the shared
// pbus, waits for the warp to halt, then reads the result back. The GPU kernel
// (gpu_demo) computes 30+4+8=42 into its scratchpad word 0; this app prints
// "GPU(42) OK\n" over UART if the readback matches.
#include "../drivers/uart.h"
#include "../drivers/gpu.h"

static void uart_uint(uint32_t v) {
    char b[12];
    unsigned n = 0;
    if (v == 0) { uart_putc('0'); return; }
    while (v) { b[n++] = '0' + (v % 10); v /= 10; }
    while (n) uart_putc(b[--n]);
}

void app_main(void) {
    uart_puts("GPU dispatch\n");
    gpu_launch(0);                 // warp0 starts at kernel pc 0
    gpu_wait_idle();               // poll active_warps until warp halts
    uint32_t r = gpu_result();     // read scratchpad word 0 back over pbus
    uart_puts("GPU(");
    uart_uint(r);
    uart_puts(r == 42 ? ") PASS\n" : ") FAIL\n");
}
