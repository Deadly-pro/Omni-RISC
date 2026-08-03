# Omni-RISC APU — GPU Kernel: relu
# Y[0..3] = max(X[0..3], 0). X @ 0, Y @ 16.
# relu(x) = x * (0 < x)  — SLT gives the 0/1 mask, MUL applies it.
LDI r1, 0       # &X
LDI r2, 16      # &Y
LDI r7, 1

LD  r3, r1      # i=0
SLT r4, r0, r3
MUL r5, r3, r4
ST  r2, r5
ADD r1, r1, r7
ADD r2, r2, r7

LD  r3, r1      # i=1
SLT r4, r0, r3
MUL r5, r3, r4
ST  r2, r5
ADD r1, r1, r7
ADD r2, r2, r7

LD  r3, r1      # i=2
SLT r4, r0, r3
MUL r5, r3, r4
ST  r2, r5
ADD r1, r1, r7
ADD r2, r2, r7

LD  r3, r1      # i=3
SLT r4, r0, r3
MUL r5, r3, r4
ST  r2, r5

HALT
