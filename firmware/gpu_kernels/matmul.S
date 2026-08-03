# Omni-RISC APU — GPU Kernel: matmul
# 2x2 matrix multiply per SIMT sub-lane: C = A * B.
# Layout (word addrs, row-major): A @ 0..3, B @ 4..7, C @ 8..11.
# c[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j] — fully unrolled.

# c00 = A[0]*B[4] + A[1]*B[6]
LDI r1, 0
LD  r2, r1
LDI r1, 1
LD  r3, r1
LDI r1, 4
LD  r4, r1
LDI r1, 6
LD  r5, r1
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 8
ST  r1, r2

# c01 = A[0]*B[5] + A[1]*B[7]
LDI r1, 0
LD  r2, r1
LDI r1, 1
LD  r3, r1
LDI r1, 5
LD  r4, r1
LDI r1, 7
LD  r5, r1
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 9
ST  r1, r2

# c10 = A[2]*B[4] + A[3]*B[6]
LDI r1, 2
LD  r2, r1
LDI r1, 3
LD  r3, r1
LDI r1, 4
LD  r4, r1
LDI r1, 6
LD  r5, r1
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 10
ST  r1, r2

# c11 = A[2]*B[5] + A[3]*B[7]
LDI r1, 2
LD  r2, r1
LDI r1, 3
LD  r3, r1
LDI r1, 5
LD  r4, r1
LDI r1, 7
LD  r5, r1
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 11
ST  r1, r2

HALT
