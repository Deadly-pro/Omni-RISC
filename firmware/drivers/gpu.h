// Omni-RISC APU — gpu driver header
#ifndef GPU_H
#define GPU_H

#include <stdint.h>

// GPU command registers (pbus slave @0x4000_2000)
#define GPU_BASE   0x40002000
#define GPU_WARP_PC0 (GPU_BASE + 0x00)
#define GPU_LAUNCH   (GPU_BASE + 0x10)  // bit31 = go, bit[1:0] = warp id
#define GPU_RESULT   (GPU_BASE + 0x14)  // warp0 scratchpad word 0
#define GPU_STATUS   (GPU_BASE + 0x18)  // (unlisted -> returns active_warps)

void     gpu_launch(uint32_t pc);
void     gpu_wait_idle(void);
uint32_t gpu_result(void);

#endif