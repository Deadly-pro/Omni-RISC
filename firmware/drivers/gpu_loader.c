/* Omni-RISC APU - runtime GPU kernel loading (firmware track, F8).
 * Spec: firmware/drivers/gpu_loader.h.
 */
#include "gpu_loader.h"
#include "gpu.h"

#define MTIME_LO_R  (*(volatile uint32_t *)0x0200BFF8u)
#define CLINT_MSIP  (*(volatile uint32_t *)0x02000000u)
#define TICKS_PER_US 50u

static volatile unsigned irq_count;
static volatile int      irq_ran;
static volatile int      done_flag;

/* Machine software interrupt: the GPU's completion. Read STATUS first - that
 * clears the GPU's done latch combinationally - and then clear msip, or the
 * level stays high and the interrupt re-enters forever. */
void omni_msi_isr(void)
{
    (void)*(volatile uint32_t *)GPU_STATUS;
    CLINT_MSIP = 0;
    irq_count++;
    irq_ran = 1;
    done_flag = 1;
}

void gpu_load_kernel(const uint16_t *words, uint32_t len)
{
    *(volatile uint32_t *)GPU_IMEM_ADDR = 0;              /* start at word 0 */
    for (uint32_t i = 0; i < len; i++)
        *(volatile uint32_t *)GPU_IMEM_DATA = words[i];   /* install + addr++ */
}

uint32_t gpu_imem_addr(void)
{
    return *(volatile uint32_t *)GPU_IMEM_ADDR & 0x3FFu;
}

int gpu_run_wait_irq(uint32_t pc, uint32_t timeout_us)
{
    uint32_t t0;

    done_flag = 0;
    irq_count = 0;
    irq_ran   = 0;

    /* MSIE: the completion arrives as a machine software interrupt. MIE too,
     * since the loader must not silently depend on some other init having run. */
    __asm__ volatile("csrs mie, %0"     :: "r"(0x8u));
    __asm__ volatile("csrs mstatus, %0" :: "r"(0x8u));

    gpu_launch(pc);

    t0 = MTIME_LO_R;
    while (!done_flag) {
        if ((uint32_t)(MTIME_LO_R - t0) > timeout_us * TICKS_PER_US)
            return 0;                                    /* timed out */
    }
    return 1;
}

unsigned gpu_irq_count(void) { return irq_count; }
int gpu_irq_handler_ran(void) { return irq_ran; }
