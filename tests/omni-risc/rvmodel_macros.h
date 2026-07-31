// rvmodel_macros.h — DUT-specific macros for the Omni-RISC RV32IM CPU
//
// This is the "golden" macros for the CPU under test (Verilator sim).
// RVMODEL_HALT_PASS/FAIL are empty: rvtest_setup.h emits a `j .` after them,
// so a passing test spins at rvmodel_halt_pass and a failing test spins at
// rvmodel_halt_fail. The Verilator harness detects which loop the PC entered
// and dumps the signature region.
#ifndef RVMODEL_MACROS_H
#define RVMODEL_MACROS_H

#define RVMODEL_DATA_SECTION \
        .pushsection .data,"aw",@progbits; \
        .popsection

/* No supervisor/trap framework — these tests run bare M-mode. */
// #define STANDARD_SM_SUPPORTED

##### STARTUP #####
// No custom boot or IO needed.
// #define RVMODEL_BOOT
// #define RVMODEL_BOOT_TO_MMODE

##### TERMINATION #####
// Empty: rvtest_setup.h's `j .` creates the spin loop the harness detects.
#define RVMODEL_HALT_PASS
#define RVMODEL_HALT_FAIL

##### IO #####
// No UART in the bare CPU sim — a no-op.
#define RVMODEL_IO_INIT(_R1, _R2, _R3)
#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR) \
        j 1f ; \
1:

##### Access Fault #####
#define RVMODEL_ACCESS_FAULT_ADDRESS 0x00000100

##### Machine Timer / Interrupts #####
// No timer or PLIC in the CPU — these satisfy check_defines.h and stay inert.
#define RVMODEL_MTIME_ADDRESS 0x0200BFF8
#define RVMODEL_MTIMECMP_ADDRESS 0x02004000
#define RVMODEL_INTERRUPT_LATENCY 10
#define RVMODEL_TIMER_INT_SOON_DELAY 100
#define RVMODEL_MAX_CYCLES_PER_TIMER_TICK 1

#define RVMODEL_SET_MEXT_INT(_R1, _R2)
#define RVMODEL_CLR_MEXT_INT(_R1, _R2)
#define RVMODEL_SET_MSW_INT(_R1, _R2)
#define RVMODEL_CLR_MSW_INT(_R1, _R2)
#define RVMODEL_SET_SEXT_INT(_R1, _R2)
#define RVMODEL_CLR_SEXT_INT(_R1, _R2)
#define RVMODEL_SET_SSW_INT(_R1, _R2)
#define RVMODEL_CLR_SSW_INT(_R1, _R2)

#endif
