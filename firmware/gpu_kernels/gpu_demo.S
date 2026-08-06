# Omni-RISC APU — GPU kernel for the coherent-APU demo (benchmark_gpu)
# End-to-end shared-memory vector add: the CPU writes A[0..3] and B[0..3] into
# warp0's scratchpad over the host window (A @ word 0, B @ word 16), launches
# this kernel, then reads C[0..3] back (C @ word 32). No copies — the scratchpad
# is the unified memory the CPU and GPU share.
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
