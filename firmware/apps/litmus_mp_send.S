// Litmus: message passing — sender (core0)
//   data at 0x2000, flag at 0x3000, VALUE = 0x5A5 (testbench patches the addi)
//   sw VALUE, 0x2000 ; fence ; li t3,1 ; sw t3, 0x3000
    .section .text
    .global _start
_start:
    li   t0, 0x2000       # data addr
    li   t1, 0x3000       # flag addr
    li   t2, 0x5A5        # VALUE (testbench patches this addi)
    sw   t2, 0(t0)        # data = VALUE
    fence
    li   t3, 1
    sw   t3, 0(t1)        # flag = 1
halt:
    j    halt
