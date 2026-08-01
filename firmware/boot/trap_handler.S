# Omni-RISC APU — Trap Handler
# Saves the caller-visible registers, passes mcause to timer_int_handler(),
# restores, and mret's.
.section .text.trap
.globl _trap_handler
_trap_handler:
    addi sp, sp, -128
    sw   x1,   0(sp)
    sw   x5,   4(sp)
    sw   x6,   8(sp)
    sw   x7,  12(sp)
    sw   x8,  16(sp)
    sw   x9,  20(sp)
    sw   x10, 24(sp)
    sw   x11, 28(sp)
    sw   x12, 32(sp)
    sw   x13, 36(sp)
    sw   x14, 40(sp)
    sw   x15, 44(sp)
    sw   x16, 48(sp)
    sw   x17, 52(sp)
    sw   x18, 56(sp)
    sw   x19, 60(sp)
    sw   x20, 64(sp)
    sw   x21, 68(sp)
    sw   x22, 72(sp)
    sw   x23, 76(sp)
    sw   x24, 80(sp)
    sw   x25, 84(sp)
    sw   x26, 88(sp)
    sw   x27, 92(sp)
    sw   x28, 96(sp)
    sw   x29, 100(sp)
    sw   x30, 104(sp)
    sw   x31, 108(sp)

    csrr a0, mcause
    call timer_int_handler

    lw   x1,   0(sp)
    lw   x5,   4(sp)
    lw   x6,   8(sp)
    lw   x7,  12(sp)
    lw   x8,  16(sp)
    lw   x9,  20(sp)
    lw   x10, 24(sp)
    lw   x11, 28(sp)
    lw   x12, 32(sp)
    lw   x13, 36(sp)
    lw   x14, 40(sp)
    lw   x15, 44(sp)
    lw   x16, 48(sp)
    lw   x17, 52(sp)
    lw   x18, 56(sp)
    lw   x19, 60(sp)
    lw   x20, 64(sp)
    lw   x21, 68(sp)
    lw   x22, 72(sp)
    lw   x23, 76(sp)
    lw   x24, 80(sp)
    lw   x25, 84(sp)
    lw   x26, 88(sp)
    lw   x27, 92(sp)
    lw   x28, 96(sp)
    lw   x29, 100(sp)
    lw   x30, 104(sp)
    lw   x31, 108(sp)
    addi sp, sp, 128
    mret
