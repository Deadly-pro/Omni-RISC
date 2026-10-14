# Omni-RISC APU — GPU Kernel: relu_mem
# Y[0..3] = max(X[0..3], 0) through MAIN MEMORY (LDG/STG).
# Byte offsets from GPU_MEM_BASE: X @ 0,4,8,12   Y @ 16,20,24,28.
# relu(x) = x * (0 < x) — SLT gives the 0/1 mask, MUL applies it.
# r7 is the byte stride (the ISA has r0..r7 only).
LDI r1, 0       # &X
LDI r2, 16      # &Y
LDI r7, 4       # byte stride

LDG r3, r1
SLT r4, r0, r3
MUL r5, r3, r4
STG r2, r5
ADD r1, r1, r7
ADD r2, r2, r7

LDG r3, r1
SLT r4, r0, r3
MUL r5, r3, r4
STG r2, r5
ADD r1, r1, r7
ADD r2, r2, r7

LDG r3, r1
SLT r4, r0, r3
MUL r5, r3, r4
STG r2, r5
ADD r1, r1, r7
ADD r2, r2, r7

LDG r3, r1
SLT r4, r0, r3
MUL r5, r3, r4
STG r2, r5

HALT
