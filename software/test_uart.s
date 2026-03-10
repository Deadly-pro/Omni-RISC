.section .text
.globl _start

_start:
    # Memory Map:
    # UART TX is at 0x10000000
    li t0, 0x10000000

    # Write 'O'
    li t1, 79
    sw t1, 0(t0)

    # Write 'M'
    li t1, 77
    sw t1, 0(t0)

    # Write 'N'
    li t1, 78
    sw t1, 0(t0)

    # Write 'I'
    li t1, 73
    sw t1, 0(t0)

    # Write '\n'
    li t1, 10
    sw t1, 0(t0)

loop:
    j loop
