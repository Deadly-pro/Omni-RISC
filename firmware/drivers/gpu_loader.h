/* Omni-RISC APU - runtime GPU kernel loading (firmware track, F8).
 *
 * The Phase-F demo flashed the kernel into the GPU's instruction memory at
 * synthesis time ($readmemh), which means a kernel could only ever change by
 * re-synthesising. The GPU now has a load port (gpu_cmd_proc's IMEM_ADDR /
 * IMEM_DATA registers, 0x4000_2024 / 0x4000_2028): write IMEM_ADDR once, then
 * each store to IMEM_DATA installs a 16-bit word and advances the pointer, so a
 * loop pushes a whole kernel the way memcpy would.
 *
 * And a kernel load is only half of it: the completion path here waits on the
 * interrupt (the GPU's done latch raises a machine software interrupt via the
 * CLINT's msip) instead of spinning on STATUS, which is what the flashed-kernel
 * path did.
 */
#ifndef GPU_LOADER_H
#define GPU_LOADER_H

#include <stdint.h>

#define GPU_IMEM_ADDR (0x40002024u)   /* RW: 10-bit word index into GPU imem */
#define GPU_IMEM_DATA (0x40002028u)   /* W:  install word, then addr++        */

/* Push `len` 16-bit words starting at imem word 0. */
void     gpu_load_kernel(const uint16_t *words, uint32_t len);

/* Read the load pointer back (word index). After a load of `len` words this
 * should be len (mod the 1024-word memory), so a caller can confirm the port
 * really took the writes rather than assume it. */
uint32_t gpu_imem_addr(void);

/* Launch at `pc` and wait for the completion interrupt.
 * Returns 1 when the interrupt arrived, 0 on timeout (in microseconds). */
int      gpu_run_wait_irq(uint32_t pc, uint32_t timeout_us);

unsigned gpu_irq_count(void);       /* completion interrupts seen */
int      gpu_irq_handler_ran(void); /* 1 once the MSI handler has run */

#endif /* GPU_LOADER_H */
