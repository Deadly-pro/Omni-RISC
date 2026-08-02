// Litmus: message passing — receiver (core1)
//   wait for flag == 1, then read data
//   loop: lw t2, 0x3000 ; beqz t2, loop ; lw t3, 0x2000 ; result = (t3==VALUE)
    .section .text
    .global _start
_start:
    li   t0, 0x2000       # data addr
    li   t1, 0x3000       # flag addr
    li   t5, 0x4000       # done-marker addr
poll:
    lw   t2, 0(t1)        # flag
    beqz t2, poll         # wait for sender
    lw   t3, 0(t0)        # data
    li   t4, 1
    sw   t4, 0(t5)        # done marker (core1 finished)
halt:
    j    halt
