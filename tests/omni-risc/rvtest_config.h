// rvtest_config.h — per-test config for the Omni-RISC DUT (RV32IM, M-mode)
// Manual replacement for the UDB-generated config the ACT4 framework emits.
// Covers the rv32i/rv32im test suites: base I, M, Zicsr, Zifencei.
#ifndef RVTEST_CONFIG_H
#define RVTEST_CONFIG_H

#define UDB_MXLEN 32
#define UDB_ELEN 32
#define UDB_VLEN 32
#define UDB_SEW_MIN 8

// mtvec supports DIRECT mode only (base alignment 2)
#define UDB_MTVEC_BASE_ALIGNMENT_DIRECT 2
#define UDB_MTVEC_BASE_ALIGNMENT_VECTORED 6
#define UDB_MTVEC_MODES_0

// No PMP
#define UDB_NUM_PMP_ENTRIES 0
#define UDB_NUM_USABLE_PMP_ENTRIES 0
#define UDB_PMP_GRANULARITY 0

// Implemented extensions: I, M, Zicsr, Zifencei
#define I_SUPPORTED
#define M_SUPPORTED
#define ZICSR_SUPPORTED
#define ZIFENCEI_SUPPORTED

#endif
