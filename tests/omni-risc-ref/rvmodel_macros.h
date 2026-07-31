// rvmodel_macros.h — reference macros for qemu-riscv32 (signature builds)
//
// RVMODEL_HALT_PASS dumps [begin_signature, end_signature) to "sig.out"
// via RISC-V semihosting file I/O, then exits 0x20026 (pass).
// qemu-user handles the semihosting calls; each trigger sequence must not
// cross a page boundary, hence the .balign 16 before each call.
#ifndef RVMODEL_MACROS_H
#define RVMODEL_MACROS_H

#define RVMODEL_DATA_SECTION \
        .pushsection .data,"aw",@progbits;   \
        .p2align 2;                          \
        __sh_name: .asciz "sig.out";         \
        __sh_open:  .word __sh_name; .word 7; .word 4; \
        __sh_write: .word 0; .word 0; .word 0;  \
        __sh_close: .word 0;                 \
        .popsection

// #define STANDARD_SM_SUPPORTED
// #define RVMODEL_BOOT
// #define RVMODEL_BOOT_TO_MMODE

#define RVMODEL_HALT_PASS \
  /* fill write params: buf=begin_signature, len=end-begin */ \
  la t0, __sh_write;                          \
  la t1, begin_signature;                     \
  sw t1, 4(t0);                               \
  la t1, end_signature;                       \
  la t2, begin_signature;                     \
  sub t1, t1, t2;                             \
  sw t1, 8(t0);                               \
  /* SYS_OPEN("sig.out", mode=w) */           \
  la a1, __sh_open;                           \
  li a0, 0x01;                                \
  .balign 16;                                 \
  slli x0, x0, 0x1f; ebreak; srai x0, x0, 7;  \
  sw a0, 0(t0);                               /* save fd */ \
  /* SYS_WRITE(fd, buf, len) */               \
  la a1, __sh_write;                          \
  li a0, 0x05;                                \
  .balign 16;                                 \
  slli x0, x0, 0x1f; ebreak; srai x0, x0, 7;  \
  /* SYS_CLOSE(fd) */                         \
  la a1, __sh_close;                          \
  li a0, 0x02;                                \
  .balign 16;                                 \
  slli x0, x0, 0x1f; ebreak; srai x0, x0, 7;  \
  /* SYS_EXIT(pass = 0x20026) */              \
  li a1, 0x20026;                             \
  li a0, 0x18;                                \
  .balign 16;                                 \
  slli x0, x0, 0x1f; ebreak; srai x0, x0, 7

#define RVMODEL_HALT_FAIL \
  /* SYS_EXIT(fail = 0x20023) */              \
  li a1, 0x20023;                             \
  li a0, 0x18;                                \
  .balign 16;                                 \
  slli x0, x0, 0x1f; ebreak; srai x0, x0, 7

#define RVMODEL_IO_INIT(_R1, _R2, _R3)
#define RVMODEL_IO_WRITE_STR(_R1, _R2, _R3, _STR_PTR)

#define RVMODEL_ACCESS_FAULT_ADDRESS 0x00000100

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
