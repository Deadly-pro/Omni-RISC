// =============================================================================
// tb_alu.cpp — Verilator testbench for Omni-RISC ALU
// =============================================================================
//
// DUT: alu (hardware/rtl/cpu/core/alu.v)
//
// Port map:
//   input  [31:0] operand_a, operand_b
//   input  [3:0]  alu_op
//       0=ADD, 1=SUB, 2=AND, 3=OR, 4=XOR, 5=SLT, 6=SLTU,
//       7=SLL, 8=SRL, 9=SRA, 10=LUI_PASS, 11=AUIPC
//   output [31:0] result
//   output        zero_flag
//
// This testbench is purely combinational — no clock is needed.
// Verilator trace (VCD) is enabled for waveform debugging.
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "Valu.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// ALU operation encoding — must match RTL
// ---------------------------------------------------------------------------
enum AluOp : uint8_t {
    ALU_ADD       = 0,
    ALU_SUB       = 1,
    ALU_AND       = 2,
    ALU_OR        = 3,
    ALU_XOR       = 4,
    ALU_SLT       = 5,
    ALU_SLTU      = 6,
    ALU_SLL       = 7,
    ALU_SRL       = 8,
    ALU_SRA       = 9,
    ALU_LUI_PASS  = 10,
    ALU_AUIPC     = 11,
};

// ---------------------------------------------------------------------------
// Human-readable names for operations
// ---------------------------------------------------------------------------
static const char* op_name(uint8_t op) {
    static const char* names[] = {
        "ADD", "SUB", "AND", "OR", "XOR", "SLT", "SLTU",
        "SLL", "SRL", "SRA", "LUI_PASS", "AUIPC"
    };
    if (op < 12) return names[op];
    return "???";
}

// ---------------------------------------------------------------------------
// Test vector definition
// ---------------------------------------------------------------------------
struct TestVec {
    std::string  label;       // Human-readable description
    uint32_t     a;           // operand_a
    uint32_t     b;           // operand_b
    uint8_t      op;          // alu_op
    uint32_t     exp_result;  // expected result
    bool         exp_zero;    // expected zero_flag
};

// ---------------------------------------------------------------------------
// Build test vectors
// ---------------------------------------------------------------------------
static std::vector<TestVec> make_tests() {
    std::vector<TestVec> tv;

    // ---- ADD (op=0) --------------------------------------------------------
    // Basic addition
    tv.push_back({"ADD: 5 + 3 = 8",
                   5, 3, ALU_ADD, 8, false});
    // Adding zero
    tv.push_back({"ADD: 0 + 0 = 0 (zero flag)",
                   0, 0, ALU_ADD, 0, true});
    // Overflow: 0x7FFFFFFF + 1 wraps to 0x80000000 (negative in signed)
    tv.push_back({"ADD: INT_MAX + 1 = 0x80000000 (overflow wrap)",
                   0x7FFFFFFF, 1, ALU_ADD, 0x80000000u, false});
    // Negative + positive: (-1) + 1 = 0
    tv.push_back({"ADD: -1 + 1 = 0 (zero flag)",
                   0xFFFFFFFF, 1, ALU_ADD, 0, true});
    // Large unsigned
    tv.push_back({"ADD: 0xFFFFFFFF + 0xFFFFFFFF = 0xFFFFFFFE (carry out lost)",
                   0xFFFFFFFF, 0xFFFFFFFF, ALU_ADD, 0xFFFFFFFE, false});

    // ---- SUB (op=1) --------------------------------------------------------
    tv.push_back({"SUB: 10 - 3 = 7",
                   10, 3, ALU_SUB, 7, false});
    tv.push_back({"SUB: 3 - 3 = 0 (zero flag)",
                   3, 3, ALU_SUB, 0, true});
    // Underflow: 0 - 1 = 0xFFFFFFFF
    tv.push_back({"SUB: 0 - 1 = 0xFFFFFFFF (underflow wrap)",
                   0, 1, ALU_SUB, 0xFFFFFFFF, false});
    // Signed underflow: INT_MIN - 1
    tv.push_back({"SUB: INT_MIN - 1 = 0x7FFFFFFF (signed underflow)",
                   0x80000000u, 1, ALU_SUB, 0x7FFFFFFF, false});

    // ---- AND (op=2) --------------------------------------------------------
    tv.push_back({"AND: all-ones & all-ones = all-ones",
                   0xFFFFFFFF, 0xFFFFFFFF, ALU_AND, 0xFFFFFFFF, false});
    tv.push_back({"AND: all-ones & 0 = 0 (zero flag)",
                   0xFFFFFFFF, 0, ALU_AND, 0, true});
    tv.push_back({"AND: alternating 0xAAAAAAAA & 0x55555555 = 0",
                   0xAAAAAAAA, 0x55555555, ALU_AND, 0, true});
    tv.push_back({"AND: 0xFF00FF00 & 0x0F0F0F0F = 0x0F000F00",
                   0xFF00FF00, 0x0F0F0F0F, ALU_AND, 0x0F000F00, false});

    // ---- OR (op=3) ---------------------------------------------------------
    tv.push_back({"OR: 0 | 0 = 0 (zero flag)",
                   0, 0, ALU_OR, 0, true});
    tv.push_back({"OR: alternating 0xAAAAAAAA | 0x55555555 = all-ones",
                   0xAAAAAAAA, 0x55555555, ALU_OR, 0xFFFFFFFF, false});
    tv.push_back({"OR: 0xFF000000 | 0x000000FF = 0xFF0000FF",
                   0xFF000000, 0x000000FF, ALU_OR, 0xFF0000FF, false});

    // ---- XOR (op=4) --------------------------------------------------------
    tv.push_back({"XOR: same value = 0 (zero flag)",
                   0xDEADBEEF, 0xDEADBEEF, ALU_XOR, 0, true});
    tv.push_back({"XOR: all-ones ^ 0 = all-ones",
                   0xFFFFFFFF, 0, ALU_XOR, 0xFFFFFFFF, false});
    tv.push_back({"XOR: alternating bits",
                   0xAAAAAAAA, 0x55555555, ALU_XOR, 0xFFFFFFFF, false});

    // ---- SLT signed (op=5) -------------------------------------------------
    // Signed less-than: result is 1 if a < b (signed), else 0
    tv.push_back({"SLT: -1 < 0 → 1",
                   0xFFFFFFFF, 0, ALU_SLT, 1, false});
    tv.push_back({"SLT: 0 < -1(0xFFFFFFFF) → 0 (not less)",
                   0, 0xFFFFFFFF, ALU_SLT, 0, true});
    tv.push_back({"SLT: INT_MIN < INT_MAX → 1",
                   0x80000000u, 0x7FFFFFFF, ALU_SLT, 1, false});
    tv.push_back({"SLT: 5 < 5 → 0 (equal, zero flag)",
                   5, 5, ALU_SLT, 0, true});

    // ---- SLTU unsigned (op=6) -----------------------------------------------
    tv.push_back({"SLTU: 0 < 1 → 1",
                   0, 1, ALU_SLTU, 1, false});
    tv.push_back({"SLTU: 0xFFFFFFFF < 0 → 0 (large unsigned not less)",
                   0xFFFFFFFF, 0, ALU_SLTU, 0, true});
    tv.push_back({"SLTU: 0x7FFFFFFF < 0x80000000 → 1 (unsigned)",
                   0x7FFFFFFF, 0x80000000u, ALU_SLTU, 1, false});
    tv.push_back({"SLTU: 10 < 10 → 0 (zero flag)",
                   10, 10, ALU_SLTU, 0, true});

    // ---- SLL (op=7) — shift left logical ------------------------------------
    // Only bottom 5 bits of operand_b are used as shift amount
    tv.push_back({"SLL: 1 << 0 = 1",
                   1, 0, ALU_SLL, 1, false});
    tv.push_back({"SLL: 1 << 31 = 0x80000000",
                   1, 31, ALU_SLL, 0x80000000u, false});
    tv.push_back({"SLL: 0xFF << 4 = 0xFF0",
                   0xFF, 4, ALU_SLL, 0xFF0, false});
    tv.push_back({"SLL: 0 << 15 = 0 (zero flag)",
                   0, 15, ALU_SLL, 0, true});

    // ---- SRL (op=8) — shift right logical -----------------------------------
    tv.push_back({"SRL: 0x80000000 >> 31 = 1",
                   0x80000000u, 31, ALU_SRL, 1, false});
    tv.push_back({"SRL: 0xFF >> 0 = 0xFF (no shift)",
                   0xFF, 0, ALU_SRL, 0xFF, false});
    tv.push_back({"SRL: 0xFF000000 >> 24 = 0xFF",
                   0xFF000000, 24, ALU_SRL, 0xFF, false});

    // ---- SRA (op=9) — shift right arithmetic --------------------------------
    // Sign bit is preserved
    tv.push_back({"SRA: 0x80000000 >> 31 = 0xFFFFFFFF (sign extend)",
                   0x80000000u, 31, ALU_SRA, 0xFFFFFFFF, false});
    tv.push_back({"SRA: 0x7FFFFFFF >> 31 = 0 (positive → zero flag)",
                   0x7FFFFFFF, 31, ALU_SRA, 0, true});
    tv.push_back({"SRA: 0xFFFFFFF0 >> 4 = 0xFFFFFFFF",
                   0xFFFFFFF0, 4, ALU_SRA, 0xFFFFFFFF, false});
    tv.push_back({"SRA: 0x80000000 >> 0 = 0x80000000 (no shift)",
                   0x80000000u, 0, ALU_SRA, 0x80000000u, false});

    // ---- LUI_PASS (op=10) — pass operand_b through unchanged ----------------
    tv.push_back({"LUI_PASS: pass 0xDEAD0000",
                   0x12345678, 0xDEAD0000, ALU_LUI_PASS, 0xDEAD0000, false});
    tv.push_back({"LUI_PASS: pass 0 (zero flag)",
                   0x12345678, 0, ALU_LUI_PASS, 0, true});

    // ---- AUIPC (op=11) — operand_a (PC) + operand_b (imm) ------------------
    tv.push_back({"AUIPC: PC=0x1000 + imm=0x2000 = 0x3000",
                   0x1000, 0x2000, ALU_AUIPC, 0x3000, false});
    tv.push_back({"AUIPC: PC=0 + imm=0 = 0 (zero flag)",
                   0, 0, ALU_AUIPC, 0, true});

    return tv;
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    // Instantiate DUT
    Valu* dut = new Valu;

    // ---- VCD trace setup ---------------------------------------------------
    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_alu.vcd");
#endif

    // ---- Run tests ----------------------------------------------------------
    auto tests = make_tests();
    int pass_count = 0;
    int fail_count = 0;
    vluint64_t sim_time = 0;  // "cycle" counter (one eval per test)

    printf("============================================================\n");
    printf("  Omni-RISC ALU Testbench — %zu test vectors\n", tests.size());
    printf("============================================================\n\n");

    for (size_t i = 0; i < tests.size(); i++) {
        const auto& t = tests[i];

        // Drive inputs
        dut->operand_a = t.a;
        dut->operand_b = t.b;
        dut->alu_op    = t.op;

        // Evaluate combinational logic
        dut->eval();
        tfp->dump(sim_time++);

        // Check result
        uint32_t got_result = dut->result;
        bool     got_zero   = dut->zero_flag;

        bool result_ok = (got_result == t.exp_result);
        bool zero_ok   = (got_zero == t.exp_zero);
        bool ok        = result_ok && zero_ok;

        if (ok) {
            pass_count++;
            printf("  [PASS] #%02zu %-6s | %s\n",
                   i, op_name(t.op), t.label.c_str());
        } else {
            fail_count++;
            printf("  [FAIL] #%02zu %-6s | %s\n",
                   i, op_name(t.op), t.label.c_str());
            if (!result_ok) {
                printf("         result: expected 0x%08X, got 0x%08X\n",
                       t.exp_result, got_result);
            }
            if (!zero_ok) {
                printf("         zero_flag: expected %d, got %d\n",
                       (int)t.exp_zero, (int)got_zero);
            }
        }
    }

    // ---- Summary ------------------------------------------------------------
    printf("\n============================================================\n");
    printf("  Results: %d PASSED, %d FAILED out of %zu tests\n",
           pass_count, fail_count, tests.size());
    printf("  Simulation cycles: %lu\n", (unsigned long)sim_time);
    printf("============================================================\n");

    // ---- Cleanup ------------------------------------------------------------
    tfp->close();
    delete tfp;
    delete dut;

    return (fail_count > 0) ? 1 : 0;
}
