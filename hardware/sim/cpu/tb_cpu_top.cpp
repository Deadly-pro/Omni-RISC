// =============================================================================
// tb_cpu_top.cpp — First-boot testbench for the Omni-RISC 5-stage CPU
// =============================================================================
// Strategy: write a hand-assembled program.hex, run the closed-loop CPU for
// enough cycles to retire everything, then peek the architectural regfile
// through the Verilator hierarchy (reg_file is marked /* verilator public */).
//
// The program proves, in order:
//   1. Straight-line ALU flow      (addi/addi/add with 2-NOP spacing — no fwd yet)
//   2. JAL: redirect + link value  (x5 must be pc+4, NOT the jump target)
//   3. BOTH wrong-path kills       (poison writes to x31/x30 must never retire:
//                                   one dies in IF/ID via decode flush, one at
//                                   the fetch NOP mux)
//   4. Taken BEQ: same double-kill (poison x29/x28)
//
// Registers x28-x31 are canaries: any nonzero value = a wrong-path instruction
// escaped the squash and retired. That is the bug this TB exists to catch.
// =============================================================================

#include <cstdio>
#include <cstdint>
#include "Vcpu_top.h"
#include "Vcpu_top___024root.h"
#include "Vcpu_top_cpu_top.h"
#include "Vcpu_top_decode_stage.h"
#include "Vcpu_top_regfile.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#ifndef VCD_FILE
#define VCD_FILE "tb_cpu_top.vcd"
#endif

static Vcpu_top* dut;
static VerilatedVcdC* tfp;
static vluint64_t sim_time = 0;
static int checks = 0;
static int fails = 0;

static void tick() {
    dut->clk = 0; dut->eval(); tfp->dump(sim_time++);
    dut->clk = 1; dut->eval(); tfp->dump(sim_time++);
}

static uint32_t peek_reg(int idx) {
    // /* verilator public */ on reg_file makes Verilator keep the module
    // hierarchy as real C++ cells: root -> cpu_top -> u_decode -> regfile1
    return dut->rootp->cpu_top->u_decode->regfile1->reg_file[idx];
}

static void check_reg(int idx, uint32_t expect, const char* why) {
    checks++;
    uint32_t got = peek_reg(idx);
    if (got != expect) {
        fails++;
        printf("FAIL  x%-2d = 0x%08X, expected 0x%08X  (%s)\n", idx, got, expect, why);
    } else {
        printf("pass  x%-2d = 0x%08X  (%s)\n", idx, got, why);
    }
}

int main(int argc, char** argv) {
    // program.hex must exist before the DUT is constructed — $readmemh runs
    // in the instr_bram initial block at time zero.
    {
        FILE* f = fopen("program.hex", "w");
        if (!f) { printf("ERROR: cannot write program.hex\n"); return 2; }
        uint32_t prog[1024];
        for (int i = 0; i < 1024; i++) prog[i] = 0x00000013; // NOP fill
        int i = 0;
        prog[i++] = 0x00500093; // 0x00: addi x1, x0, 5
        prog[i++] = 0x00700113; // 0x04: addi x2, x0, 7
        prog[i++] = 0x00000013; // 0x08: nop            (2-NOP rule: no forwarding)
        prog[i++] = 0x00000013; // 0x0C: nop
        prog[i++] = 0x002081B3; // 0x10: add  x3, x1, x2        -> x3 = 12
        prog[i++] = 0x00C002EF; // 0x14: jal  x5, +12           -> 0x20, link = 0x18
        prog[i++] = 0x11100F93; // 0x18: addi x31, x0, 0x111    POISON (decode-flush slot)
        prog[i++] = 0x22200F13; // 0x1C: addi x30, x0, 0x222    POISON (fetch-squash slot)
        prog[i++] = 0x00100313; // 0x20: addi x6, x0, 1         JAL target
        prog[i++] = 0x00000013; // 0x24: nop
        prog[i++] = 0x00000013; // 0x28: nop
        prog[i++] = 0x00108663; // 0x2C: beq  x1, x1, +12       taken -> 0x38
        prog[i++] = 0x33300E93; // 0x30: addi x29, x0, 0x333    POISON (decode-flush slot)
        prog[i++] = 0x44400E13; // 0x34: addi x28, x0, 0x444    POISON (fetch-squash slot)
        prog[i++] = 0x00900393; // 0x38: addi x7, x0, 9         branch target
        for (int j = 0; j < 1024; j++) fprintf(f, "%08X\n", prog[j]);
        fclose(f);
    }

    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    dut = new Vcpu_top;
    tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open(VCD_FILE);

    dut->reset = 1;
    for (int i = 0; i < 3; i++) tick();
    dut->reset = 0;

    // 15 program instructions + redirect bubbles + 5-stage drain << 80 cycles
    for (int i = 0; i < 80; i++) tick();

    printf("\n--- architectural state after run ---\n");
    check_reg( 0, 0x00000000, "x0 is hardwired zero");
    check_reg( 1, 0x00000005, "addi x1, x0, 5");
    check_reg( 2, 0x00000007, "addi x2, x0, 7");
    check_reg( 3, 0x0000000C, "add x3, x1, x2 — THE first-boot check");
    check_reg( 5, 0x00000018, "JAL link = pc+4 of the jal, via WB jump mux");
    check_reg( 6, 0x00000001, "instruction at JAL target executed");
    check_reg( 7, 0x00000009, "instruction at BEQ target executed");
    check_reg(31, 0x00000000, "JAL wrong-path #1 killed by decode flush");
    check_reg(30, 0x00000000, "JAL wrong-path #2 killed by fetch squash");
    check_reg(29, 0x00000000, "BEQ wrong-path #1 killed by decode flush");
    check_reg(28, 0x00000000, "BEQ wrong-path #2 killed by fetch squash");

    tfp->close();
    printf("\n%d/%d checks passed\n", checks - fails, checks);
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
