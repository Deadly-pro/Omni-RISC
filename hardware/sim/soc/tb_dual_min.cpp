// Minimal dual-core debug: both cores run a trivial program (addi, j .).
// Checks both cores' PCs advance — isolates the shared-memory arbiter.
#include <cstdio>
#include <cstdint>
#include "Vdual_core_top.h"
#include "Vdual_core_top___024root.h"
#include "Vdual_core_top_dual_core_top.h"
#include "Vdual_core_top_cpu_top__Dz1_U1_S1.h"
#include "Vdual_core_top_pc_gen.h"
#include "Vdual_core_top_regfile.h"
#include "Vdual_core_top_fetch_stage__U1.h"
#include "Vdual_core_top_decode_stage.h"
#include "verilated.h"

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    uint32_t prog[8] = {
        0x00100093, // addi x1,x0,1
        0x00200113, // addi x2,x0,2
        0x0000006F, // j .
    };
    FILE* f = fopen("program.hex", "w");
    for (int i = 0; i < 1024; i++) fprintf(f, "%08X\n", i < 3 ? prog[i] : 0x13);
    fclose(f);

    Vdual_core_top* dut = new Vdual_core_top;
    dut->reset = 1;
    for (int i = 0; i < 6; i++) { dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval(); }
    dut->reset = 0;
    for (int cyc = 0; cyc < 100; cyc++) {
        dut->clk = 0; dut->eval();
        dut->clk = 1; dut->eval();
        uint32_t pc0 = dut->rootp->dual_core_top->u_core0->u_fetch->pc_gen1->pc;
        uint32_t pc1 = dut->rootp->dual_core_top->u_core1->u_fetch->pc_gen1->pc;
        uint32_t x1_0 = dut->rootp->dual_core_top->u_core0->u_decode->regfile1->reg_file[1];
        uint32_t x1_1 = dut->rootp->dual_core_top->u_core1->u_decode->regfile1->reg_file[1];
        if (cyc % 20 == 0)
            printf("cyc=%3d pc0=0x%02x x1_0=%d | pc1=0x%02x x1_1=%d\n", cyc, pc0, x1_0, pc1, x1_1);
    }
    uint32_t pc0 = dut->rootp->dual_core_top->u_core0->u_fetch->pc_gen1->pc;
    uint32_t pc1 = dut->rootp->dual_core_top->u_core1->u_fetch->pc_gen1->pc;
    uint32_t x1_0 = dut->rootp->dual_core_top->u_core0->u_decode->regfile1->reg_file[1];
    uint32_t x1_1 = dut->rootp->dual_core_top->u_core1->u_decode->regfile1->reg_file[1];
    bool pass = (x1_0 == 1) && (x1_1 == 1) && (pc0 >= 0x8) && (pc1 >= 0x8);
    printf("%s: x1_0=%d x1_1=%d pc0=0x%x pc1=0x%x\n",
           pass ? "PASS" : "FAIL", x1_0, x1_1, pc0, pc1);
    return pass ? 0 : 1;
}
