# Omni-RISC APU — GPU Kernel: matmul_mem
# C = A * B through MAIN MEMORY (LDG/STG), not the scratchpad.
# Row-major, byte offsets from GPU_MEM_BASE:
#   A @ 0,4,8,12   B @ 16,20,24,28   C @ 32,36,40,44
# c[i][j] = a[i][0]*b[0][j] + a[i][1]*b[1][j], so the B column index selects every
# other word: c00 needs B[0][0]=+16 and B[1][0]=+24 (NOT +20).
# The global access is warp-uniform, so all lanes compute the same offsets and one
# bus transaction serves the warp.
LDI r1, 0
LDG r2, r1      # a00 (+0)
LDI r1, 4
LDG r3, r1      # a01 (+4)
LDI r1, 16
LDG r4, r1      # b00 (+16)
LDI r1, 24
LDG r5, r1      # b10 (+24)
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 32
STG r1, r2      # c00 (+32)

LDI r1, 0
LDG r2, r1      # a00
LDI r1, 4
LDG r3, r1      # a01
LDI r1, 20
LDG r4, r1      # b01 (+20)
LDI r1, 28
LDG r5, r1      # b11 (+28)
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 36
STG r1, r2      # c01 (+36)

LDI r1, 8
LDG r2, r1      # a10 (+8)
LDI r1, 12
LDG r3, r1      # a11 (+12)
LDI r1, 16
LDG r4, r1      # b00
LDI r1, 24
LDG r5, r1      # b10
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 40
STG r1, r2      # c10 (+40)

LDI r1, 8
LDG r2, r1      # a10
LDI r1, 12
LDG r3, r1      # a11
LDI r1, 20
LDG r4, r1      # b01
LDI r1, 28
LDG r5, r1      # b11
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 44
STG r1, r2      # c11 (+44)

HALT
