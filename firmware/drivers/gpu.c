// Omni-RISC APU — gpu driver implementation
#include "gpu.h"

#define GO      0x80000000u

void gpu_launch(uint32_t pc) {
    *(volatile uint32_t *)GPU_WARP_PC0 = pc;
    *(volatile uint32_t *)GPU_LAUNCH   = GO | 0u;  // warp 0
}

void gpu_wait_idle(void) {
    while ((*(volatile uint32_t *)GPU_STATUS & 0xFu) != 0)
        ;
}

uint32_t gpu_result(void) {
    return *(volatile uint32_t *)GPU_RESULT;
}

// shared-memory window: write a 32-bit word into warp0 scratchpad lane `bank`
// at word address `word`. The store lands before the next pbus write, so
// launching after the last input write makes the data visible to the kernel.
void gpu_write_word(uint32_t bank, uint32_t word, uint32_t v) {
    *(volatile uint32_t *)GPU_HOST_WIN  = (bank << 8) | word;
    *(volatile uint32_t *)GPU_HOST_DATA = v;
}

// read lane `bank` word `word` back from warp0 scratchpad over the window.
uint32_t gpu_read_word(uint32_t bank, uint32_t word) {
    *(volatile uint32_t *)GPU_HOST_WIN = (bank << 8) | word;
    return *(volatile uint32_t *)GPU_HOST_DATA;
}
