# Omni-RISC APU — GPU Kernel: vector_add
# C[0..3] = A[0..3] + B[0..3], per SIMT sub-lane (each lane's own scratchpad bank).
# Layout (word addrs): A @ 0, B @ 16, C @ 32.
# Unrolled x4 — the ISA has no conditional branch.
LDI r1, 0       # &A
LDI r2, 16      # &B
LDI r3, 32      # &C
LDI r7, 1       # stride

LD  r4, r1      # i=0
LD  r5, r2
ADD r6, r4, r5
ST  r3, r6
ADD r1, r1, r7
ADD r2, r2, r7
ADD r3, r3, r7

LD  r4, r1      # i=1
LD  r5, r2
ADD r6, r4, r5
ST  r3, r6
ADD r1, r1, r7
ADD r2, r2, r7
ADD r3, r3, r7

LD  r4, r1      # i=2
LD  r5, r2
ADD r6, r4, r5
ST  r3, r6
ADD r1, r1, r7
ADD r2, r2, r7
ADD r3, r3, r7

LD  r4, r1      # i=3
LD  r5, r2
ADD r6, r4, r5
ST  r3, r6

HALT
