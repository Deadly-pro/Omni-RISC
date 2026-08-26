# Omni-RISC APU — GPU Kernel: barrier (4-warp marker sync test)
#
# All four warps run the same code at the same PC. Each warp writes a PRE
# marker to its own scratchpad (bank l word 0 = 0x55), hits the BARRIER,
# then reads word 0 back (post-barrier load proof) and stores it to word 1,
# writes a POST marker to word 2, and halts.
#
# The TB verifies that every warp's scratchpad holds:
#   word 0 = 0x55 (PRE written before the barrier)
#   word 1 = 0x55 (post-barrier load — warp resumed)
#   word 2 = 0xAA (POST — warp completed the full kernel)
# Deadlock = active_warps never returns to zero.
#
# NOTE: r0 is hardwired to zero in gpu_regfile — never use it as a target
# or address register. This kernel uses r2/r3/r4.
LDI r2, 0
LDI r3, 0x55
ST  r2, r3        # sp[0] = PRE 0x55
BARRIER
LDI r2, 0
LD  r4, r2        # r4 = sp[0] (post-barrier read)
LDI r2, 1
ST  r2, r4        # sp[1] = 0x55 (post-barrier load proof)
LDI r2, 2
LDI r3, 0xAA
ST  r2, r3        # sp[2] = POST 0xAA
HALT