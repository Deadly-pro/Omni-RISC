# Omni-RISC APU — Trap Handler
# TODO: Implement
# - Save all registers to stack
# - Read mcause to determine interrupt type
# - Dispatch to appropriate handler
# - Restore registers, mret
.section .text.trap
.globl _trap_handler
_trap_handler:
    # TODO: implement
    mret
