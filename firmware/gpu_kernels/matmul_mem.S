# Omni-RISC APU — GPU Kernel: matmul_mem
# C = A * B through MAIN MEMORY (LDG/STG), not the scratchpad.
# Byte offsets from GPU_MEM_BASE: A @ 0,4,8,12  B @ 16,20,24,28  C @ 32,36,40,44.
# Applied to a 2x2 row-major matrix per SIMT lane; the global access is
# warp-uniform, so all lanes compute the same offsets and one bus transaction
# serves the warp.
LDI r1, 0
LDG r2, r1      # A[0]
LDI r1, 4
LDG r3, r1      # A[1]
LDI r1, 16
LDG r4, r1      # B[0]
LDI r1, 20
LDG r5, r1      # B[1]
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 32
STG r1, r2      # C[0]

LDI r1, 0
LDG r2, r1
LDI r1, 4
LDG r3, r1
LDI r1, 20
LDG r4, r1
LDI r1, 24
LDG r5, r1
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 36
STG r1, r2      # C[1]

LDI r1, 8
LDG r2, r1
LDI r1, 12
LDG r3, r1
LDI r1, 16
LDG r4, r1
LDI r1, 20
LDG r5, r1
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 40
STG r1, r2      # C[2]

LDI r1, 8
LDG r2, r1
LDI r1, 12
LDG r3, r1
LDI r1, 20
LDG r4, r1
LDI r1, 24
LDG r5, r1
MUL r2, r2, r4
MUL r3, r3, r5
ADD r2, r2, r3
LDI r1, 44
STG r1, r2      # C[3]

HALT
