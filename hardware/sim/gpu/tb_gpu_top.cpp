// =============================================================================
// tb_gpu_top.cpp — Verilator testbench for gpu_top (full GPU)
// =============================================================================
// End-to-end: poke kernels into imem, launch warps via PBUS, check scratchpad
// results and halt.
//
// ISA (16-bit): [15:12]op [11:9]rd [8:6]rs1 [5:3]rs2 [2:0]f3
//   0=ALU(f3)  1=LSU(bit0: 0=load rd<-sp[rs1], 1=store sp[rs1]<-rs2)
//   2=BR(target=instr[7:0], byte addr)  3=MUL  4=ALU2(f3: 0=SRL 1=SRA 2=MUL)
//   5=LDI(rd<-imm9)  F=HALT
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vgpu_top.h"
#include "Vgpu_top_gpu_top.h"
#include "Vgpu_top_gpu_fetch.h"
#include "Vgpu_top_exec_lane.h"
#include "Vgpu_top_gpu_scratchpad.h"
#include "verilated.h"

static int checks = 0, fails = 0;
static void check(const char* label, uint32_t expect, uint32_t got) {
    checks++;
    if (expect == got) { printf("  pass  %s\n", label); }
    else { fails++; printf("  FAIL  %s — expected 0x%08X, got 0x%08X\n", label, expect, got); }
}

// encoders
static uint16_t ALU(int rd,int rs1,int rs2,int f3){ return (0x0<<12)|(rd<<9)|(rs1<<6)|(rs2<<3)|f3; }
static uint16_t LD (int rd,int rs1)               { return (0x1<<12)|(rd<<9)|(rs1<<6); }
static uint16_t ST (int rs1,int rs2)              { return (0x1<<12)|(rs1<<6)|(rs2<<3)|1; }
static uint16_t BR (int target)                   { return (0x2<<12)|(target&0xFF); }
static uint16_t MUL(int rd,int rs1,int rs2)       { return (0x4<<12)|(rd<<9)|(rs1<<6)|(rs2<<3)|2; }
static uint16_t LDI(int rd,int imm9)              { return (0x5<<12)|(rd<<9)|(imm9&0x1FF); }
static const uint16_t HALT = 0xF000;

static Vgpu_top* dut;
static void tick() { dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval(); }

static void pbus_write(uint32_t addr, uint32_t data) {
    dut->pbus_addr = addr; dut->pbus_wdata = data; dut->pbus_wen = 0xF;
    tick();
    dut->pbus_wen = 0;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    dut = new Vgpu_top;

    // reset
    dut->reset = 1; tick(); dut->reset = 0;

    auto* imem = &dut->gpu_top->u_fetch->imem;
    Vgpu_top_gpu_scratchpad* sp[4] = {
        dut->gpu_top->g_warp__BRA__0__KET____DOT__u_lane->u_sp,
        dut->gpu_top->g_warp__BRA__1__KET____DOT__u_lane->u_sp,
        dut->gpu_top->g_warp__BRA__2__KET____DOT__u_lane->u_sp,
        dut->gpu_top->g_warp__BRA__3__KET____DOT__u_lane->u_sp,
    };

    // ---- Warp 0 kernel @ 0x00: r3 = 5+7 = 12; store sp[5]; load back; store sp[7] ----
    int pc = 0;
    (*imem)[pc++] = LDI(1, 5);      // r1 = 5
    (*imem)[pc++] = LDI(2, 7);      // r2 = 7
    (*imem)[pc++] = ALU(3, 1, 2, 0);// r3 = r1 + r2 = 12
    (*imem)[pc++] = ST(1, 3);       // sp[5] = 12
    (*imem)[pc++] = LD(4, 1);       // r4 = sp[5] = 12
    (*imem)[pc++] = ST(2, 4);       // sp[7] = 12
    (*imem)[pc++] = HALT;

    // ---- Warp 1 kernel @ 0x40 (word 0x20): r3 = 3*4 = 12; store sp[3] ----
    pc = 0x20;
    (*imem)[pc++] = LDI(1, 3);      // r1 = 3
    (*imem)[pc++] = LDI(2, 4);      // r2 = 4
    (*imem)[pc++] = MUL(3, 1, 2);   // r3 = 12
    (*imem)[pc++] = ST(1, 3);       // sp[3] = 12
    (*imem)[pc++] = HALT;

    // ---- Warp 2 kernel @ 0x80 (word 0x40): branch to 0x10 where HALT waits ----
    (*imem)[0x40] = BR(0x10);
    (*imem)[0x10 >> 1] = HALT;

    // ---- Launch all three warps via PBUS ----
    pbus_write(0x40002000, 0x000);       // warp_pc[0] = 0x00
    pbus_write(0x40002004, 0x040);       // warp_pc[1] = 0x40
    pbus_write(0x40002008, 0x080);       // warp_pc[2] = 0x80
    pbus_write(0x40002010, 0x80000000);  // launch warp 0
    pbus_write(0x40002010, 0x80000001);  // launch warp 1
    pbus_write(0x40002010, 0x80000002);  // launch warp 2
    tick();  // cmd_launch is registered — 1 cycle for the last launch to land

    check("active_warps after launch", 0x7, dut->active_warps);

    // run until all warps halt (rr issue = 1 instr per warp per 4 cycles)
    int cyc = 0;
    while (dut->active_warps && cyc < 200) { tick(); cyc++; }
    printf("  (all warps idle after %d cycles)\n", cyc);
    check("all warps halted", 0, dut->active_warps);

    // ---- Warp 0 results: sp[5]=12 and sp[7]=12 in every bank ----
    check("w0 bank0[5]", 12, sp[0]->bank0[5]);
    check("w0 bank1[5]", 12, sp[0]->bank1[5]);
    check("w0 bank2[5]", 12, sp[0]->bank2[5]);
    check("w0 bank3[5]", 12, sp[0]->bank3[5]);
    check("w0 bank0[7] (load->store)", 12, sp[0]->bank0[7]);
    check("w0 bank3[7] (load->store)", 12, sp[0]->bank3[7]);

    // ---- Warp 1 results: sp[3]=12 (MUL) ----
    check("w1 bank0[3]", 12, sp[1]->bank0[3]);
    check("w1 bank3[3]", 12, sp[1]->bank3[3]);

    // ---- Warp isolation: warp1's store must not land in warp0's scratchpad ----
    check("w0 bank0[3] untouched", 0, sp[0]->bank0[3]);

    // ---- PBUS readback: warp_pc[1] ----
    dut->pbus_addr = 0x40002004; dut->pbus_read = 1; dut->eval();
    check("pbus readback warp_pc[1]", 0x40, dut->pbus_rdata);
    dut->pbus_read = 0;

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
