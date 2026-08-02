// =============================================================================
// tb_lrsc.cpp — Single-core LR.W/SC.W test (USE_CACHES=1)
// =============================================================================
// Verifies the A-extension LR/SC path: decode → exec → mem_stage → dcache → WB.
//
// Program (assembled by hand):
//   0x00: li   x1, 0x100      # lock address in data window
//   0x04: li   x2, 0x55       # value to store
//   0x08: lr.w x3, (x1)       # load-reserve, x3 = old value
//   0x0c: sc.w x4, x2, (x1)   # store-conditional, x4 = 0 on success
//   0x10: j .                 # halt
//
// Encodings:
//   LR.W x3,(x1):  funct5=00010 aq=0 rl=0 rs2=0 rs1=1 funct3=010 rd=3 op=0101111
//                  0x1000A2AF
//   SC.W x4,x2,(x1): funct5=00011 aq=0 rl=0 rs2=2 rs1=1 funct3=010 rd=4 op=0101111
//                  0x1820A22F
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vcpu_top.h"
#include "Vcpu_top___024root.h"
#include "Vcpu_top_cpu_top.h"
#include "Vcpu_top_decode_stage.h"
#include "Vcpu_top_regfile.h"
#include "Vcpu_top_pc_gen.h"
#if __has_include("Vcpu_top_fetch_stage__U1.h")
#include "Vcpu_top_fetch_stage__U1.h"
#else
#include "Vcpu_top_fetch_stage.h"
#endif
#if __has_include("Vcpu_top_mem_stage__U1.h")
#include "Vcpu_top_mem_stage__U1.h"
#endif
#if __has_include("Vcpu_top_data_bram.h")
#include "Vcpu_top_data_bram.h"
#endif
#include "verilated.h"
#include "verilated_vcd_c.h"

static int checks = 0;
static int fails = 0;

static void check_reg(Vcpu_top* dut, int idx, uint32_t expect, const char* why) {
    checks++;
    uint32_t got = dut->rootp->cpu_top->u_decode->regfile1->reg_file[idx];
    if (got != expect) {
        fails++;
        printf("FAIL  x%-2d = 0x%08X, expected 0x%08X  (%s)\n", idx, got, expect, why);
    } else {
        printf("pass  x%-2d = 0x%08X  (%s)\n", idx, got, why);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    uint32_t prog[32] = {
        0x10000093, // 0x00: li   x1, 0x100   (addi x1,x0,0x100)
        0x05500113, // 0x04: li   x2, 0x55    (addi x2,x0,0x55)
        0x1000A2AF, // 0x08: lr.w x3, (x1)
        0x1820A22F, // 0x0c: sc.w x4, x2, (x1)
        0x0000006F, // 0x10: j .
    };
    FILE* f = fopen("program.hex", "w");
    for (int i = 0; i < 1024; i++)
        fprintf(f, "%08X\n", i < 5 ? prog[i] : 0x00000013u);
    fclose(f);

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    Vcpu_top* dut = new Vcpu_top;
    dut->trace(tfp, 99);
    tfp->open("tb_lrsc.vcd");

    dut->reset = 1;
    for (int i = 0; i < 6; i++) { dut->clk = 0; dut->eval(); tfp->dump(i*20); dut->clk = 1; dut->eval(); tfp->dump(i*20+10); }
    dut->reset = 0;

    for (int cyc = 0; cyc < 400; cyc++) {
        dut->clk = 0; dut->eval(); tfp->dump((6+cyc)*20);
        dut->clk = 1; dut->eval(); tfp->dump((6+cyc)*20+10);
        // halt check: pc stuck at 0x14+ (past the j .) or regs settled
        uint32_t pc = dut->rootp->cpu_top->u_fetch->pc_gen1->pc;
        uint32_t x4 = dut->rootp->cpu_top->u_decode->regfile1->reg_file[4];
        if (cyc > 100 && x4 <= 1) break;  // SC result visible
    }

    printf("\n--- LR/SC single-core test ---\n");
    check_reg(dut, 1, 0x100, "x1 = lock address");
    check_reg(dut, 2, 0x55, "x2 = value to store");
    check_reg(dut, 3, 0x00000000, "x3 = LR reads old lock value (0)");
    check_reg(dut, 4, 0x00000000, "x4 = SC success (0)");

    // verify the SC.W write-through landed in data_bram
    checks++;
    uint32_t memval = dut->rootp->cpu_top->u_mem->u_dbram->mem[0x100 >> 2];
    if (memval != 0x55) {
        fails++;
        printf("FAIL  mem[0x100] = 0x%08X, expected 0x00000055  (SC write-through)\n", memval);
    } else {
        printf("pass  mem[0x100] = 0x00000055  (SC write-through landed)\n");
    }

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    tfp->close();
    return 0;
}
