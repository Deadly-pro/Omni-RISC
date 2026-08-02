// Omni-RISC APU — dual-core spinlock self-test
//
// Both cores run this identical image. A shared lock (0x1000) protects a
// shared counter (0x1004). Each core repeatedly: LR.W/SC.W acquire, increment,
// release — until the counter reaches TARGET. A correct spinlock means no lost
// updates: the final counter equals TARGET. If the lock is broken (both cores
// inside the critical section), increments are lost and counter < TARGET.
//
// Register map:
//   s0 = lock address (0x1000)   s1 = counter address (0x1004)
//   t1 = 1 (lock value)          t6 = TARGET (1000)
    .section .text
    .global _start
_start:
    li    s0, 0x1000
    li    s1, 0x1004
    li    t1, 1
    li    t6, 1000

retry:
    // acquire: LR.W/SC.W spinlock
    lr.w  t2, (s0)          // t2 = old lock (reserve addr)
    bnez  t2, retry         // lock held → retry
    sc.w  t3, t1, (s0)      // try to set lock
    bnez  t3, retry         // lost race → retry
have_lock:
    lw    t4, 0(s1)         // counter
    addi  t4, t4, 1
    sw    t4, 0(s1)
    sw    zero, 0(s0)       // release lock
    blt   t4, t6, retry     // counter < TARGET → keep going
halt:
    j     halt
