# Omni-RISC APU — GPU Kernel: mem_add
# C = A + B, where A, B and C live in MAIN MEMORY (the CPU's own data_bram), not
# in the scratchpad. Read with LDG, write with STG; the base address comes from
# the GPU_MEM_BASE register (0x4000_202C), which the CPU sets to point at its
# array, so these are small offsets from that base.
#
# A @ +0, B @ +4, C @ +8 (bytes; LDG/STG take a full 32-bit byte address in rs1).
# The access is warp-uniform: every lane computes the same address, so one bus
# transaction serves the warp and the loaded word is broadcast.
LDI r1, 0       # &A
LDI r2, 4       # &B
LDI r3, 8       # &C

LDG r4, r1      # r4 = mem[base+0]
LDG r5, r2      # r5 = mem[base+4]
ADD r6, r4, r5  # r6 = A + B
STG r3, r6      # mem[base+8] = r6

HALT
