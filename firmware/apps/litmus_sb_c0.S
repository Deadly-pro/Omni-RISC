// Litmus: store buffering — core0
//   write X(0x1000)=1, then read Y(0x2000) into x3, store x3 at 0x4000
    .section .text
    .global _start
_start:
    li   t0, 0x1000       # X
    li   t1, 0x2000       # Y
    li   t2, 1
    sw   t2, 0(t0)        # X = 1
    lw   t3, 0(t1)        # read Y
    li   t5, 0x4000       # result addr
    sw   t3, 0(t5)        # record Y-view
halt:
    j    halt
