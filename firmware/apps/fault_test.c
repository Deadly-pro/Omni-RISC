/**
 * @file fault_test.c
 * @brief F6 gate app: fault diagnostics (firmware track).
 *
 * One fault per run, because a fault has no fix-up path here - the handler
 * dumps and parks. tests/test_fault.sh therefore runs this app once per kind
 * (via APP_DEFS) and asserts what each run produced.
 *
 *   -DFAULT_KIND=0   illegal instruction (.word 0)
 *   -DFAULT_KIND=1   misaligned load  (the only access fault this core can raise:
 *                    there is no MMU/PMP, so an unmapped address just answers)
 *   -DFAULT_KIND=2   ecall from M-mode -> the deliberate semihost exit
 *   -DFAULT_KIND=3   ebreak
 *
 * If a kind turns out not to trap on this RTL, the app says so explicitly
 * ("fault none") instead of the gate having to guess.
 */
#include <stdint.h>
#include <stddef.h>
#include "../drivers/uart.h"
#include <omni/stdio.h>

#ifndef FAULT_KIND
#define FAULT_KIND 0
#endif

static void trigger(void)
{
#if FAULT_KIND == 0
    __asm__ volatile(".word 0x00000000");          /* illegal opcode */
#elif FAULT_KIND == 1
    {
        volatile uint32_t v = 0x11223344u;
        volatile uint8_t *p = (volatile uint8_t *)&v;
        volatile uint32_t *bad = (volatile uint32_t *)(p + 1);   /* odd address */
        v = *bad;
        (void)v;
    }
#elif FAULT_KIND == 2
    __asm__ volatile("ecall");
#elif FAULT_KIND == 3
    __asm__ volatile("ebreak");
#endif
}

void app_main(void)
{
    uart_init();
    printf("[TEST] READY\r\n");
    printf("[TEST] fault gate kind=%d\r\n", (int)FAULT_KIND);

    trigger();

    /* Reached only when the RTL does not raise anything for this kind. */
    printf("[TEST] fault none kind=%d (no trap was taken)\r\n", (int)FAULT_KIND);
    printf("[TEST] SUMMARY 1/1\r\n");
    printf("[TEST] DONE\r\n");
    for (;;) { }
}
