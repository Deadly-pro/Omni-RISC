/* Omni-RISC APU - fault diagnostics (firmware track, F6).
 *
 * Until now every trap went to timer_int_handler(), so an illegal instruction or
 * a misaligned load silently became "advance the timer" - the firmware carried
 * on with corrupted state and no clue. This splits the two: interrupts still go
 * to the timer handler, exceptions get decoded and dumped, and an ecall from
 * M-mode is a deliberate semihost exit (which is how a test can end cleanly on
 * purpose rather than by running off the end).
 *
 * Note what this core *can* raise: there is no MMU/PMP, so "access fault" cases
 * (load/store from an unmapped address) do not exist here - the bus simply
 * answers. Illegal instruction, misaligned access, breakpoint and ecall are the
 * ones that exist, and the gate records which of those the RTL actually raises.
 *
 * The register frame is the one boot/trap_handler.S builds: 4-byte slots holding
 * x1 at offset 0, then x5..x31 at 4..108 (28 words), so frame[0] is ra and
 * frame[1+k] is x(5+k).
 */
#include <stdint.h>
#include <stddef.h>
#include <omni/stdio.h>
#include "../drivers/uart.h"
#include "../drivers/timer.h"
#include "../drivers/tick.h"

static unsigned faults;

static const char *const regnames[28] = {
    "ra", "t0", "t1", "t2", "s0", "s1", "a0", "a1", "a2", "a3", "a4", "a5",
    "a6", "a7", "s2", "s3", "s4", "s5", "s6", "s7", "s8", "s9", "s10", "s11",
    "t3", "t4", "t5", "t6"
};

static const char *cause_name(uint32_t mc)
{
    if (mc & 0x80000000u) {
        switch (mc & 0xFFu) {
        case 3:  return "machine software interrupt";
        case 7:  return "machine timer interrupt";
        case 11: return "machine external interrupt";
        default: return "interrupt";
        }
    }
    switch (mc & 0x7FFFFFFFu) {
    case 0:  return "instruction address misaligned";
    case 1:  return "instruction access fault";
    case 2:  return "illegal instruction";
    case 3:  return "breakpoint";
    case 4:  return "load address misaligned";
    case 5:  return "load access fault";
    case 6:  return "store address misaligned";
    case 7:  return "store access fault";
    case 11: return "ecall from M-mode";
    default: return "unknown";
    }
}

unsigned fault_count(void) { return faults; }

void fault_report(uint32_t mcause, uint32_t mepc, uint32_t mtval,
                  const uint32_t *frame)
{
    faults++;
    printf("[FAULT] count=%u mcause=0x%08x name=%s\r\n",
           faults, (unsigned)mcause, cause_name(mcause));
    printf("[FAULT] mepc=0x%08x mtval=0x%08x\r\n", (unsigned)mepc, (unsigned)mtval);
    for (unsigned i = 0; i < 28; i++) {
        if ((i % 4u) == 0u) printf("[FAULT] ");
        printf("%s=0x%08x ", regnames[i], (unsigned)frame[i]);
        if ((i % 4u) == 3u) printf("\r\n");
    }
    printf("\r\n");

    if ((mcause & 0x80000000u) == 0u && (mcause & 0x7FFFFFFFu) == 11u) {
        /* ecall from M-mode: a deliberate "stop here", not a crash. The harness
         * ends the simulation on the marker. */
        printf("[FAULT] ecall -> semihost exit\r\n");
        printf("[TEST] DONE\r\n");
    }

    /* An exception has no fix-up path on this core (no MMU, no handler that can
     * resume), so park rather than return into a corrupt machine. */
    for (;;) { }
}

/* Called from boot/trap_handler.S with the saved frame. */
void trap_dispatch(uint32_t mcause, uint32_t mepc, uint32_t mtval,
                   const uint32_t *frame)
{
    if (mcause & 0x80000000u) {
        /* A machine timer interrupt goes to the software timer service when it is
         * running; an app that programs the CLINT itself keeps the old path. */
        if ((mcause & 0xFFu) == 7u && tick_enabled()) {
            tick_isr();
            return;
        }
        timer_int_handler(mcause);      /* interrupts keep their old path */
        return;
    }
    fault_report(mcause, mepc, mtval, frame);
}
