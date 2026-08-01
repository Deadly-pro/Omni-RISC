// Focused: 2 stores then a JAL on the cached CPU (USE_CACHES=1)
// 0x00 addi x2,x0,0x100
// 0x04 addi x1,x0,0x55
// 0x08 sw   x1,0(x2)   store 1 (cold miss)
// 0x0c sw   x1,4(x2)   store 2 (hit)
// 0x10 jal  x0,+12     -> 0x1c  (0x14,0x18 must be skipped)
// 0x14 addi x3,x0,2    (skip)
// 0x18 addi x4,x0,4    (skip)
// 0x1c addi x6,x0,5    target
// 0x20 j .
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
#include "verilated.h"
#include "verilated_vcd_c.h"

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    uint32_t prog[16] = {
        0x10000113, // 0x00 addi x2,x0,0x100
        0x05500093, // 0x04 addi x1,x0,0x55
        0x0020A023, // 0x08 sw x1,0(x2)
        0x0020A223, // 0x0c sw x1,4(x2)
        0x00c0006f, // 0x10 jal x0,+12 -> 0x1c
        0x00200193, // 0x14 addi x3,x0,2
        0x00400213, // 0x18 addi x4,x0,4
        0x00500313, // 0x1c addi x6,x0,5
        0x0000006f, // 0x20 j .
    };
    FILE* f = fopen("program.hex", "w");
    for (int i = 0; i < 512; i++)
        fprintf(f, "%08X\n", i < 9 ? prog[i] : 0x00000013u);
    fclose(f);

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    Vcpu_top* dut = new Vcpu_top;
    dut->trace(tfp, 99);
    tfp->open("tb_2store.vcd");
    dut->reset = 1;
    for (int i = 0; i < 6; i++) { dut->clk = 0; dut->eval(); tfp->dump(i*20); dut->clk = 1; dut->eval(); tfp->dump(i*20+10); }
    dut->reset = 0;
    for (int cyc = 0; cyc < 120; cyc++) {
        dut->clk = 0; dut->eval(); tfp->dump((6+cyc)*20);
        dut->clk = 1; dut->eval(); tfp->dump((6+cyc)*20+10);
        uint32_t pc = dut->rootp->cpu_top->u_fetch->pc_gen1->pc;
        uint32_t rv = dut->rootp->cpu_top->u_fetch->pc_gen1->__PVT__redirect_valid;
        if (cyc < 70)
            printf("cyc=%3d pc=0x%02x rv=%d\n", cyc, pc, rv);
    }
    printf("FINAL x1=0x%x x2=0x%x x3=%d x4=%d x6=%d (expect x1=0x55 x2=0x100 x3=0 x4=0 x6=5)\n",
           (int)dut->rootp->cpu_top->u_decode->regfile1->reg_file[1],
           (int)dut->rootp->cpu_top->u_decode->regfile1->reg_file[2],
           (int)dut->rootp->cpu_top->u_decode->regfile1->reg_file[3],
           (int)dut->rootp->cpu_top->u_decode->regfile1->reg_file[4],
           (int)dut->rootp->cpu_top->u_decode->regfile1->reg_file[6]);
    tfp->close();
    return 0;
}
