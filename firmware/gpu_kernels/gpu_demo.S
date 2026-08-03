# Omni-RISC APU — GPU kernel for the CPU-dispatch demo (benchmark_gpu)
# Self-contained: computes 30 + 4 + 8 = 42 and stores it to scratchpad word 0.
# The CPU reads that word back over pbus (gpu_cmd_proc +0x14 result readback)
# after polling active_warps to zero, proving end-to-end CPU <-> GPU dispatch.
LDI r1, 30
LDI r2, 4
ADD r3, r1, r2      # r3 = 34
LDI r4, 8
ADD r3, r3, r4      # r3 = 42
LDI r5, 0           # scratchpad word 0
ST  r5, r3          # sp[0] = 42
HALT