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
