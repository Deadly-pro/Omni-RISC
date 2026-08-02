// =============================================================================
// tb_gpu_regfile.cpp — Verilator testbench for the SIMT register file
// =============================================================================
// 8 vector regs × 4 lanes × 32 bits. Tests write/read, r0 hardwired zero.
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vgpu_regfile.h"
#include "verilated.h"

static int checks = 0, fails = 0;
static void check(const char* label, uint32_t expect, uint32_t got) {
    checks++;
    if (expect == got) { printf("  pass  %s\n", label); }
    else { fails++; printf("  FAIL  %s — expected 0x%08X, got 0x%08X\n", label, expect, got); }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vgpu_regfile* dut = new Vgpu_regfile;

    // reset
    dut->reset = 1; dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->reset = 0;

    // write r3 = {lane0=1, lane1=2, lane2=3, lane3=4}
    dut->rd_write_en = 1;
    dut->rd_addr = 3;
    dut->rd_data.at(0) = 1; dut->rd_data.at(1) = 2; dut->rd_data.at(2) = 3; dut->rd_data.at(3) = 4;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->rd_write_en = 0;

    // write r5 = {lane0=0xAA, lane1=0xBB, lane2=0xCC, lane3=0xDD}
    dut->rd_write_en = 1;
    dut->rd_addr = 5;
    dut->rd_data.at(0) = 0xAA; dut->rd_data.at(1) = 0xBB; dut->rd_data.at(2) = 0xCC; dut->rd_data.at(3) = 0xDD;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->rd_write_en = 0;

    // read r3 and r5
    dut->rs1_addr = 3; dut->rs2_addr = 5;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    check("rs1(r3) lane0", 1, dut->rs1_data.at(0));
    check("rs1(r3) lane3", 4, dut->rs1_data.at(3));
    check("rs2(r5) lane0", 0xAA, dut->rs2_data.at(0));
    check("rs2(r5) lane2", 0xCC, dut->rs2_data.at(2));

    // r0 is hardwired zero
    dut->rs1_addr = 0;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    check("r0 lane0 zero", 0, dut->rs1_data.at(0));
    check("r0 lane3 zero", 0, dut->rs1_data.at(3));

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
