// =============================================================================
// tb_litmus_sb.cpp — Store-buffering litmus test (MSI coherence)
// =============================================================================
// Core0: X(0x1000)=1; then read Y(0x2000) → 0x4000
// Core1: Y(0x2000)=1; then read X(0x1000) → 0x4004
//
// Forbidden outcome (SB): core0 sees Y=0 AND core1 sees X=0 — both stores
// "buffered", neither visible to the other. With correct MSI write-invalidate
// coherence, at least one core must observe the other's write (because the
// second read's refill happens after the first write is globally visible).
//
// Run many iterations; FAIL if the forbidden outcome ever occurs.
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vdual_core_top.h"
#include "Vdual_core_top___024root.h"
#include "Vdual_core_top_dual_core_top.h"
#include "Vdual_core_top_cpu_top__Dz1_U1_S1.h"
#include "Vdual_core_top_instr_bram.h"
#include "Vdual_core_top_data_bram__Mz1.h"
#include "Vdual_core_top_fetch_stage__U1.h"
#include "verilated.h"

static const uint32_t RES0_ADDR = 0x4000;
static const uint32_t RES1_ADDR = 0x4004;
static const uint64_t MAX_CYCLES = 200000;
static const int      ITERATIONS = 200;

// core0: from litmus_sb_c0.S
static const uint32_t PROG0[] = {
    0x000012b7,  // lui t0, 0x1       (X)
    0x00002337,  // lui t1, 0x2       (Y)
    0x00100393,  // li  t2, 1
    0x0072a023,  // sw  t2, 0(t0)     (X=1)
    0x00032e03,  // lw  t3, 0(t1)     (read Y)
    0x00004f37,  // lui t5, 0x4       (result)
    0x01cf2023,  // sw  t3, 0(t5)     (record Y-view)
    0x0000006f,  // j halt
};
// core1: from litmus_sb_c1.S
static const uint32_t PROG1[] = {
    0x000022b7,  // lui t0, 0x2       (Y)
    0x00001337,  // lui t1, 0x1       (X)
    0x00100393,  // li  t2, 1
    0x0072a023,  // sw  t2, 0(t0)     (Y=1)
    0x00032e03,  // lw  t3, 0(t1)     (read X)
    0x00004f37,  // lui t5, 0x4
    0x004f0f13,  // addi t5, t5, 4    (0x4004)
    0x01cf2023,  // sw  t3, 0(t5)     (record X-view)
    0x0000006f,  // j halt
};

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vdual_core_top* dut = new Vdual_core_top;

    dut->clk = 0; dut->eval();   // trigger $readmemh before pokes

    int forbidden = 0;
    for (int it = 0; it < ITERATIONS; it++) {
        for (int i = 0; i < (int)(sizeof PROG0/sizeof PROG0[0]); i++)
            dut->rootp->dual_core_top->u_core0->u_fetch->instr_bram1->mem[i] = PROG0[i];
        for (int i = (int)(sizeof PROG0/sizeof PROG0[0]); i < 256; i++)
            dut->rootp->dual_core_top->u_core0->u_fetch->instr_bram1->mem[i] = 0x0000006f;
        for (int i = 0; i < (int)(sizeof PROG1/sizeof PROG1[0]); i++)
            dut->rootp->dual_core_top->u_core1->u_fetch->instr_bram1->mem[i] = PROG1[i];
        for (int i = (int)(sizeof PROG1/sizeof PROG1[0]); i < 256; i++)
            dut->rootp->dual_core_top->u_core1->u_fetch->instr_bram1->mem[i] = 0x0000006f;

        // clear shared data
        dut->rootp->dual_core_top->u_shared_dbram->mem[0x1000>>2] = 0;
        dut->rootp->dual_core_top->u_shared_dbram->mem[0x2000>>2] = 0;
        dut->rootp->dual_core_top->u_shared_dbram->mem[RES0_ADDR>>2] = 0;
        dut->rootp->dual_core_top->u_shared_dbram->mem[RES1_ADDR>>2] = 0;

        dut->reset = 1;
        for (int i = 0; i < 4; i++) { dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval(); }
        dut->reset = 0;

        // run until both cores record their results (or timeout)
        uint64_t cyc = 0;
        uint32_t r0 = 0, r1 = 0;
        while (cyc < MAX_CYCLES) {
            dut->clk = 0; dut->eval();
            dut->clk = 1; dut->eval();
            cyc++;
            if (cyc % 10000 == 0) {
                r0 = dut->rootp->dual_core_top->u_shared_dbram->mem[RES0_ADDR>>2];
                r1 = dut->rootp->dual_core_top->u_shared_dbram->mem[RES1_ADDR>>2];
                // results recorded when BOTH non-zero-able... use "both cores finished"
                // heuristic: result values written (0 or 1) — detect both written
                uint32_t x = dut->rootp->dual_core_top->u_shared_dbram->mem[0x1000>>2];
                uint32_t y = dut->rootp->dual_core_top->u_shared_dbram->mem[0x2000>>2];
                // core finished when pc at halt; use data written as proxy: if
                // both cores' result locations have been written (X and Y both 1,
                // results set), break
                if (x == 1 && y == 1) {
                    // give a few more cycles for the result stores, then break
                    if (cyc % 50000 == 0) break;
                }
            }
        }
        r0 = dut->rootp->dual_core_top->u_shared_dbram->mem[RES0_ADDR>>2];
        r1 = dut->rootp->dual_core_top->u_shared_dbram->mem[RES1_ADDR>>2];

        // forbidden outcome: both cores read 0
        if (r0 == 0 && r1 == 0) {
            forbidden++;
            printf("ITER %d FORBIDDEN: core0 saw Y=%u, core1 saw X=%u\n", it, r0, r1);
        }
    }

    printf("Store-buffering litmus: %d iterations, %d forbidden outcomes\n", ITERATIONS, forbidden);
    if (forbidden) { printf("TB RESULT: FAIL — SB forbidden outcome observed (coherence broken).\n"); return 1; }
    printf("TB RESULT: PASS — no SB forbidden outcome.\n");
    return 0;
}
