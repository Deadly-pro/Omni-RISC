// Litmus: store buffering — core1
//   write Y(0x2000)=1, then read X(0x1000) into x3, store x3 at 0x4004
    .section .text
    .global _start
_start:
    li   t0, 0x2000       # Y
    li   t1, 0x1000       # X
    li   t2, 1
    sw   t2, 0(t0)        # Y = 1
    lw   t3, 0(t1)        # read X
    li   t5, 0x4004       # result addr
    sw   t3, 0(t5)        # record X-view
halt:
    j    halt
