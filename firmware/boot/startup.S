# Omni-RISC APU — Assembly Startup
#   - set stack pointer to _stack_top
#   - zero BSS
#   - call main
.section .text.startup
.globl _start
_start:
    la   sp, _stack_top

    # zero .bss
    la   t0, _bss_start
    la   t1, _bss_end
1:
    bgeu t0, t1, 2f
    sw   zero, 0(t0)
    addi t0, t0, 4
    j    1b
2:
    call main
1:
    j    1b      # main should not return
