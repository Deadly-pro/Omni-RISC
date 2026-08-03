// =============================================================================
// tb_exec_lane.cpp — Verilator testbench for exec_lane (4 SIMT sub-lanes)
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vexec_lane.h"
#include "verilated.h"

static int checks = 0, fails = 0;
static void check(const char* label, uint32_t expect, uint32_t got) {
    checks++;
    if (expect == got) { printf("  pass  %s\n", label); }
    else { fails++; printf("  FAIL  %s — expected 0x%08X, got 0x%08X\n", label, expect, got); }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vexec_lane* dut = new Vexec_lane;

    // reset
    dut->reset = 1; dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->reset = 0;

    // --- Test 1: regfile write/read back ---
    // write r3 = {lane0=10, lane1=20, lane2=30, lane3=40}
    dut->rd_write_en = 1;
    dut->rd_addr = 3;
    dut->rd_data.at(0) = 10; dut->rd_data.at(1) = 20; dut->rd_data.at(2) = 30; dut->rd_data.at(3) = 40;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->rd_write_en = 0;

    // read back r3: rs1=r3, rs2=r0, ADD
    dut->instr_valid = 1;
    dut->instr = 0x0000;  // ADD = alu_op 0
    dut->rs1_addr = 3;
    dut->rs2_addr = 0;
    dut->rd_write_en = 0;
    dut->clk = 0; dut->eval();
    check("regfile r3 lane0", 10, dut->alu_result.at(0));
    check("regfile r3 lane1", 20, dut->alu_result.at(1));
    check("regfile r3 lane2", 30, dut->alu_result.at(2));
    check("regfile r3 lane3", 40, dut->alu_result.at(3));
    dut->clk = 1; dut->eval();
    dut->instr_valid = 0;

    // write r4 = {lane0=1, lane1=2, lane2=3, lane3=4}
    dut->rd_write_en = 1;
    dut->rd_addr = 4;
    dut->rd_data.at(0) = 1; dut->rd_data.at(1) = 2; dut->rd_data.at(2) = 3; dut->rd_data.at(3) = 4;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->rd_write_en = 0;

    // read back r4
    dut->instr_valid = 1;
    dut->instr = 0x0000;
    dut->rs1_addr = 4;
    dut->rs2_addr = 0;
    dut->rd_write_en = 0;
    dut->clk = 0; dut->eval();
    check("regfile r4 lane0", 1, dut->alu_result.at(0));
    check("regfile r4 lane1", 2, dut->alu_result.at(1));
    check("regfile r4 lane2", 3, dut->alu_result.at(2));
    check("regfile r4 lane3", 4, dut->alu_result.at(3));
    dut->clk = 1; dut->eval();
    dut->instr_valid = 0;

    // --- Test 2: ALU ADD r3 + r4 -> r5 ---
    // set up ALU operation (combinational)
    dut->instr_valid = 1;
    dut->instr = 0x0000;  // ADD = alu_op 0
    dut->rs1_addr = 3;
    dut->rs2_addr = 4;
    dut->rd_write_en = 0;
    dut->clk = 0; dut->eval();  // check alu_result now (combinational)
    check("ADD alu_result lane0", 11, dut->alu_result.at(0));
    check("ADD alu_result lane1", 22, dut->alu_result.at(1));
    check("ADD alu_result lane2", 33, dut->alu_result.at(2));
    check("ADD alu_result lane3", 44, dut->alu_result.at(3));

    // now write result to r5: feed alu_result into rd_data
    dut->rd_addr = 5;
    dut->rd_data = dut->alu_result;  // copy ALU result to regfile write port
    dut->rd_write_en = 1;
    dut->clk = 1; dut->eval();  // posedge: write r5
    dut->rd_write_en = 0;
    dut->instr_valid = 0;

    // read back r5: rs1=r5, rs2=r0
    dut->instr_valid = 1;
    dut->instr = 0x0000;
    dut->rs1_addr = 5;
    dut->rs2_addr = 0;
    dut->rd_write_en = 0;
    dut->clk = 0; dut->eval();
    check("ADD r5 lane0", 11, dut->alu_result.at(0));
    check("ADD r5 lane1", 22, dut->alu_result.at(1));
    check("ADD r5 lane2", 33, dut->alu_result.at(2));
    check("ADD r5 lane3", 44, dut->alu_result.at(3));
    dut->clk = 1; dut->eval();
    dut->instr_valid = 0;

    // --- Test 3: ALU MUL r3 * r4 -> r6 ---
    dut->instr_valid = 1;
    dut->instr = 0x000A;  // MUL = alu_op 10
    dut->rs1_addr = 3;
    dut->rs2_addr = 4;
    dut->rd_write_en = 0;
    dut->clk = 0; dut->eval();  // check alu_result now
    check("MUL alu_result lane0", 10, dut->alu_result.at(0));
    check("MUL alu_result lane1", 40, dut->alu_result.at(1));
    check("MUL alu_result lane2", 90, dut->alu_result.at(2));
    check("MUL alu_result lane3", 160, dut->alu_result.at(3));

    // write to r6
    dut->rd_addr = 6;
    dut->rd_data = dut->alu_result;
    dut->rd_write_en = 1;
    dut->clk = 1; dut->eval();  // posedge: write r6
    dut->rd_write_en = 0;
    dut->instr_valid = 0;

    // read back r6
    dut->instr_valid = 1;
    dut->instr = 0x0000;  // ADD with r0
    dut->rs1_addr = 6;
    dut->rs2_addr = 0;
    dut->rd_write_en = 0;
    dut->clk = 0; dut->eval();
    check("MUL r6 lane0", 10, dut->alu_result.at(0));
    check("MUL r6 lane1", 40, dut->alu_result.at(1));
    check("MUL r6 lane2", 90, dut->alu_result.at(2));
    check("MUL r6 lane3", 160, dut->alu_result.at(3));
    dut->clk = 1; dut->eval();
    dut->instr_valid = 0;

    // --- Test 4: Scratchpad write/read ---
    dut->sp_write_en = 1;
    dut->sp_waddr = (0x30<<24) | (0x20<<16) | (0x10<<8) | 0x00;
    dut->sp_wdata.at(0) = 0x1111; dut->sp_wdata.at(1) = 0x2222; dut->sp_wdata.at(2) = 0x3333; dut->sp_wdata.at(3) = 0x4444;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->sp_write_en = 0;

    dut->sp_raddr = (0x30<<24) | (0x20<<16) | (0x10<<8) | 0x00;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    check("SP read lane0", 0x1111, dut->sp_rdata.at(0));
    check("SP read lane1", 0x2222, dut->sp_rdata.at(1));
    check("SP read lane2", 0x3333, dut->sp_rdata.at(2));
    check("SP read lane3", 0x4444, dut->sp_rdata.at(3));

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}