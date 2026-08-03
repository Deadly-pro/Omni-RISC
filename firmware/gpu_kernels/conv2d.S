# Omni-RISC APU — GPU Kernel: conv2d
# 1D 3-tap convolution (the "2d" is one output row): Y[i] = sum(X[i+k]*W[k]).
# X @ 0..5 (6 samples), W @ 8..10 (3 taps), Y @ 16..19 (4 outputs).
# Fully unrolled — no conditional branch in the ISA.

# load taps once: r5=W0 r6=W1 r7=W2
LDI r1, 8
LD  r5, r1
LDI r1, 9
LD  r6, r1
LDI r1, 10
LD  r7, r1

# Y[0] = X0*W0 + X1*W1 + X2*W2
LDI r1, 0
LD  r2, r1
MUL r2, r2, r5
LDI r1, 1
LD  r3, r1
MUL r3, r3, r6
ADD r2, r2, r3
LDI r1, 2
LD  r3, r1
MUL r3, r3, r7
ADD r2, r2, r3
LDI r1, 16
ST  r1, r2

# Y[1] = X1*W0 + X2*W1 + X3*W2
LDI r1, 1
LD  r2, r1
MUL r2, r2, r5
LDI r1, 2
LD  r3, r1
MUL r3, r3, r6
ADD r2, r2, r3
LDI r1, 3
LD  r3, r1
MUL r3, r3, r7
ADD r2, r2, r3
LDI r1, 17
ST  r1, r2

# Y[2] = X2*W0 + X3*W1 + X4*W2
LDI r1, 2
LD  r2, r1
MUL r2, r2, r5
LDI r1, 3
LD  r3, r1
MUL r3, r3, r6
ADD r2, r2, r3
LDI r1, 4
LD  r3, r1
MUL r3, r3, r7
ADD r2, r2, r3
LDI r1, 18
ST  r1, r2

# Y[3] = X3*W0 + X4*W1 + X5*W2
LDI r1, 3
LD  r2, r1
MUL r2, r2, r5
LDI r1, 4
LD  r3, r1
MUL r3, r3, r6
ADD r2, r2, r3
LDI r1, 5
LD  r3, r1
MUL r3, r3, r7
ADD r2, r2, r3
LDI r1, 19
ST  r1, r2

HALT
