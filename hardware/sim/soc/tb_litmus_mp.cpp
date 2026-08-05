// =============================================================================
// tb_litmus_mp.cpp — Message-passing litmus test (MSI coherence)
// =============================================================================
// Core0 (sender):  sw VALUE, data(0x2000); fence; sw 1, flag(0x3000)
// Core1 (receiver): poll flag==1; then read data(0x2000)
//
// The coherence contract (message passing): once the flag is visible (==1),
// the data write must ALSO be visible. If MSI coherence is broken, core1 can
// see flag==1 but stale data. Run many iterations with different VALUES.
//
// Programs are poked into each core's private instr_bram; data/flag live in
// the SHARED data_bram.
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
#include "Vdual_core_top_pc_gen.h"
#include "verilated.h"
#include "verilated_vcd_c.h"
#include "verilated_vcd_c.h"

static const uint32_t DATA_ADDR = 0x2000;
static const uint32_t FLAG_ADDR = 0x3000;
static const uint32_t DONE_ADDR = 0x4000;
static const uint64_t MAX_CYCLES = 200000;
static const int      ITERATIONS = 200;

// sender: from litmus_mp_send.S
static const uint32_t SEND[] = {
    0x000022b7,  // lui t0, 0x2          (data addr)
    0x00003337,  // lui t1, 0x3          (flag addr)
    0x5a500393,  // li  t2, 0x5A5        (VALUE — patched per iteration)
    0x0072a023,  // sw  t2, 0(t0)
    0x0ff0000f,  // fence
    0x00100e13,  // li  t3, 1
    0x01c32023,  // sw  t3, 0(t1)
    0x0000006f,  // j halt
};
// receiver: from litmus_mp_recv.S
static const uint32_t RECV[] = {
    0x000022b7,  // lui t0, 0x2          (data addr)
    0x00003337,  // lui t1, 0x3          (flag addr)
    0x00004f37,  // lui t5, 0x4          (done addr)
    0x00032383,  // lw  t2, 0(t1)        (flag)
    0xfe038ee3,  // beqz t2, -4          (poll)
    0x0002ae03,  // lw  t3, 0(t0)        (data)
    0x00100e93,  // li  t4, 1
    0x01df2023,  // sw  t4, 0(t5)        (done)
    0x0000006f,  // j halt
};

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    Vdual_core_top* dut = new Vdual_core_top;
    dut->trace(tfp, 99);
    tfp->open("litmus_mp.vcd");

    int fails = 0;
    for (int it = 0; it < ITERATIONS; it++) {
        uint32_t value = 1 + (it * 7) % 0x7FF;   // 12-bit range for addi

        // Verilator runs the $readmemh initial block at the FIRST eval, which
        // would overwrite our pokes with program.hex — so eval once first.
        if (it == 0) { dut->clk = 0; dut->eval(); }

        // poke per-core programs into each core's private instr_bram; fill the
        // REST with `j 0x1c` so a stale fetch-ahead never executes leftover
        // program.hex data-access code (which would hog the shared memory bus
        // and starve the other core's cache refill)
        for (int i = 0; i < (int)(sizeof SEND/sizeof SEND[0]); i++) {
            uint32_t w = SEND[i];
            if (i == 2) w = (value << 20) | 0x393;   // patch addi t2(x7), x0, VALUE
            dut->rootp->dual_core_top->u_core0->u_fetch->instr_bram1->mem[i] = w;
        }
        for (int i = (int)(sizeof SEND/sizeof SEND[0]); i < 256; i++)
            dut->rootp->dual_core_top->u_core0->u_fetch->instr_bram1->mem[i] = 0x0000006f; // j 0x1c
        for (int i = 0; i < (int)(sizeof RECV/sizeof RECV[0]); i++)
            dut->rootp->dual_core_top->u_core1->u_fetch->instr_bram1->mem[i] = RECV[i];
        for (int i = (int)(sizeof RECV/sizeof RECV[0]); i < 256; i++)
            dut->rootp->dual_core_top->u_core1->u_fetch->instr_bram1->mem[i] = 0x0000006f; // j 0x1c

        // clear shared data
        dut->rootp->dual_core_top->u_shared_dbram->mem[DATA_ADDR>>2] = 0;
        dut->rootp->dual_core_top->u_shared_dbram->mem[FLAG_ADDR>>2]  = 0;
        dut->rootp->dual_core_top->u_shared_dbram->mem[DONE_ADDR>>2]  = 0;

        if (it == 0) {
            printf("  core0 ib[0..7]:");
            for (int i = 0; i < 8; i++) printf(" %08x", dut->rootp->dual_core_top->u_core0->u_fetch->instr_bram1->mem[i]);
            printf("\n  core1 ib[0..8]:");
            for (int i = 0; i < 9; i++) printf(" %08x", dut->rootp->dual_core_top->u_core1->u_fetch->instr_bram1->mem[i]);
            printf("\n");
        }

        // reset + run until receiver marks done
        dut->reset = 1;
        for (int i = 0; i < 4; i++) { dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval(); }
        dut->reset = 0;

        uint64_t cyc = 0;
        uint32_t done = 0;
        while (cyc < MAX_CYCLES) {
            dut->clk = 0; dut->eval(); tfp->dump(cyc * 2);
            dut->clk = 1; dut->eval(); tfp->dump(cyc * 2 + 1);
            cyc++;
            if (it == 0 && cyc % 10000 == 0) {
                uint32_t pc0 = dut->rootp->dual_core_top->u_core0->u_fetch->pc_gen1->pc;
                uint32_t pc1 = dut->rootp->dual_core_top->u_core1->u_fetch->pc_gen1->pc;
                uint32_t dd = dut->rootp->dual_core_top->u_shared_dbram->mem[DATA_ADDR>>2];
                uint32_t ff = dut->rootp->dual_core_top->u_shared_dbram->mem[FLAG_ADDR>>2];
                printf("  [it0 cyc=%llu] pc0=0x%02x pc1=0x%02x data=0x%x flag=%u\n",
                       (unsigned long long)cyc, pc0, pc1, dd, ff);
            }
            if (cyc % 5000 == 0) {
                done = dut->rootp->dual_core_top->u_shared_dbram->mem[DONE_ADDR>>2];
                if (done) break;
            }
        }
        done = dut->rootp->dual_core_top->u_shared_dbram->mem[DONE_ADDR>>2];
        uint32_t flag = dut->rootp->dual_core_top->u_shared_dbram->mem[FLAG_ADDR>>2];
        uint32_t data = dut->rootp->dual_core_top->u_shared_dbram->mem[DATA_ADDR>>2];

        bool ok = (done == 1) && (flag == 1) && (data == value);
        if (!ok) {
            fails++;
            printf("ITER %d FAIL: value=0x%x done=%u flag=%u data=0x%x\n",
                   it, value, done, flag, data);
        }
    }

    printf("Message-passing litmus: %d iterations, %d failures\n", ITERATIONS, fails);
    tfp->close();
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS — flag-ordering holds, data always visible with flag.\n");
    return 0;
}
