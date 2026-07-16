// =============================================================================
// tb_branch_unit.cpp — Verilator testbench for Omni-RISC Branch Unit
// =============================================================================
//
// DUT: branch_unit (hardware/rtl/cpu/core/branch_unit.v)
//
// Port map (RTL must conform to this):
//   input  [31:0] rs1_data      // post-forwarding value
//   input  [31:0] rs2_data      // post-forwarding value
//   input  [2:0]  funct3        // which comparison (B-type encoding)
//   input         is_branch     // from decoder
//   input         is_jump       // JAL/JALR — unconditionally taken
//   output        take_branch
//
// take_branch = is_jump || (is_branch && condition(funct3, rs1, rs2))
//
// funct3: 000=BEQ 001=BNE 100=BLT(signed) 101=BGE(signed)
//         110=BLTU(unsigned) 111=BGEU(unsigned)
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vbranch_unit.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

static int pass_count = 0;
static int fail_count = 0;
static vluint64_t sim_time = 0;

struct BrTest {
    const char* name;
    uint32_t rs1, rs2;
    uint32_t funct3;
    bool     is_branch, is_jump;
    bool     expected;
};

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vbranch_unit* dut = new Vbranch_unit;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_branch_unit.vcd");
#endif

    static const BrTest tests[] = {
        // --- BEQ (000) ---
        {"BEQ equal taken",            5,          5,          0, true,  false, true },
        {"BEQ unequal not taken",      5,          6,          0, true,  false, false},
        {"BEQ both zero taken",        0,          0,          0, true,  false, true },
        // --- BNE (001) ---
        {"BNE unequal taken",          5,          6,          1, true,  false, true },
        {"BNE equal not taken",        7,          7,          1, true,  false, false},
        // --- BLT (100, signed) ---
        {"BLT 1<2 taken",              1,          2,          4, true,  false, true },
        {"BLT 2<1 not taken",          2,          1,          4, true,  false, false},
        {"BLT equal not taken",        3,          3,          4, true,  false, false},
        {"BLT -1<1 taken (signed!)",   0xFFFFFFFF, 1,          4, true,  false, true },
        {"BLT INT_MIN<INT_MAX taken",  0x80000000, 0x7FFFFFFF, 4, true,  false, true },
        // --- BGE (101, signed) ---
        {"BGE 2>=1 taken",             2,          1,          5, true,  false, true },
        {"BGE equal taken",            4,          4,          5, true,  false, true },
        {"BGE 1>=-1 taken (signed!)",  1,          0xFFFFFFFF, 5, true,  false, true },
        {"BGE -1>=1 not taken",        0xFFFFFFFF, 1,          5, true,  false, false},
        // --- BLTU (110, unsigned) ---
        {"BLTU 1<2 taken",             1,          2,          6, true,  false, true },
        {"BLTU 0xFFFFFFFF<1 NOT taken (unsigned!)",
                                       0xFFFFFFFF, 1,          6, true,  false, false},
        {"BLTU 1<0xFFFFFFFF taken",    1,          0xFFFFFFFF, 6, true,  false, true },
        {"BLTU equal not taken",       9,          9,          6, true,  false, false},
        // --- BGEU (111, unsigned) ---
        {"BGEU 0xFFFFFFFF>=1 taken",   0xFFFFFFFF, 1,          7, true,  false, true },
        {"BGEU 1>=0xFFFFFFFF not taken", 1,        0xFFFFFFFF, 7, true,  false, false},
        {"BGEU equal taken",           8,          8,          7, true,  false, true },
        // --- is_branch gating ---
        {"condition true but is_branch=0 → not taken",
                                       5,          5,          0, false, false, false},
        // --- jumps: unconditional, ignore comparison ---
        {"jump taken regardless of values",
                                       1,          2,          0, false, true,  true },
        {"jump taken even with failing condition",
                                       5,          6,          0, false, true,  true },
        // --- neither branch nor jump ---
        {"plain ALU op → never taken", 5,          5,          0, false, false, false},
    };
    const int NUM = sizeof(tests) / sizeof(tests[0]);

    printf("============================================================\n");
    printf("  Omni-RISC Branch Unit Testbench — %d test vectors\n", NUM);
    printf("============================================================\n\n");

    for (int i = 0; i < NUM; i++) {
        const BrTest& t = tests[i];
        dut->rs1_data  = t.rs1;
        dut->rs2_data  = t.rs2;
        dut->funct3    = t.funct3;
        dut->is_branch = t.is_branch;
        dut->is_jump   = t.is_jump;
        dut->eval();
        tfp->dump(sim_time++);

        if (dut->take_branch == (t.expected ? 1 : 0)) {
            pass_count++;
            printf("  [PASS] #%02d | %s\n", i, t.name);
        } else {
            fail_count++;
            printf("  [FAIL] #%02d | %s — expected %d, got %d\n",
                   i, t.name, t.expected ? 1 : 0, dut->take_branch);
        }
    }

    printf("\n============================================================\n");
    printf("  Results: %d PASSED, %d FAILED out of %d tests\n",
           pass_count, fail_count, NUM);
    printf("============================================================\n");

    tfp->close();
    delete tfp;
    delete dut;

    return (fail_count > 0) ? 1 : 0;
}
