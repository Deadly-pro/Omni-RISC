// =============================================================================
// tb_gpu_scratchpad.cpp — Verilator testbench for the 4-bank SIMT scratchpad
// =============================================================================
// Tests: write/read round-trip, per-lane bank independence, reset.
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vgpu_scratchpad.h"
#include "verilated.h"

static int checks = 0, fails = 0;
static void check(const char* label, uint32_t expect, uint32_t got) {
    checks++;
    if (expect == got) { printf("  pass  %s\n", label); }
    else { fails++; printf("  FAIL  %s — expected 0x%08X, got 0x%08X\n", label, expect, got); }
}

static uint32_t lane(const Vgpu_scratchpad* dut, int idx) {
    return dut->rdata.at(idx);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vgpu_scratchpad* dut = new Vgpu_scratchpad;

    // reset
    dut->reset = 1; dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->reset = 0;

    // write: lane0→addr10=0xAAAA, lane1→addr20=0xBBBB, lane2→addr30=0xCCCC, lane3→addr40=0xDDDD
    dut->write_en = 1;
    dut->waddr = (40u<<24) | (30u<<16) | (20u<<8) | 10u;
    dut->wdata.at(0) = 0xAAAA; dut->wdata.at(1) = 0xBBBB; dut->wdata.at(2) = 0xCCCC; dut->wdata.at(3) = 0xDDDD;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->write_en = 0;

    // read back
    dut->raddr = (40u<<24) | (30u<<16) | (20u<<8) | 10u;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    check("lane0 read-back", 0xAAAA, lane(dut, 0));
    check("lane1 read-back", 0xBBBB, lane(dut, 1));
    check("lane2 read-back", 0xCCCC, lane(dut, 2));
    check("lane3 read-back", 0xDDDD, lane(dut, 3));

    // bank independence: write only lane2's bank at addr31
    dut->write_en = 1;
    dut->waddr = (31u<<16);   // lane2 addr=31, others addr=0
    dut->wdata.at(2) = 0x1234;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    dut->write_en = 0;

    // read addr31 on all lanes: only lane2 should see the write
    dut->raddr = (31u<<24) | (31u<<16) | (31u<<8) | 31u;
    dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval();
    check("lane0 bank isolated (0)", 0, lane(dut, 0));
    check("lane2 bank written", 0x1234, lane(dut, 2));

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
