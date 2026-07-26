// =============================================================================
// tb_divider.cpp — unit TB for the RV32M iterative divider (start/busy/done)
// =============================================================================
// Drives the handshake: present operands + pulse start for one cycle, spin on
// busy, capture result on done. Operands are held stable for the whole run
// (b_mag is consumed combinationally inside the DIVIDE loop, and in the pipeline
// ID/EX is frozen during the stall — same contract).
//
// Covers all four ops, both spec fast-paths (div-by-zero, INT_MIN/-1 overflow),
// and every sign quadrant of DIV/REM (truncate toward zero; remainder takes the
// dividend's sign).
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <climits>
#include "Vdivider.h"
#include "verilated.h"

static int checks = 0, fails = 0;

static Vdivider* dut;
static vluint64_t tk = 0;

static void tick() {
    dut->clk = 0; dut->eval();
    dut->clk = 1; dut->eval();
    tk++;
}

// Reference model — RISC-V M semantics.
static uint32_t golden(uint32_t a, uint32_t b, int f3) {
    bool is_rem = (f3 >> 1) & 1;
    bool is_uns =  f3 & 1;
    if (b == 0) return is_rem ? a : 0xFFFFFFFFu;
    if (!is_uns) {
        int32_t sa = (int32_t)a, sb = (int32_t)b;
        if (sa == INT32_MIN && sb == -1)          // signed overflow
            return is_rem ? 0u : 0x80000000u;
        return is_rem ? (uint32_t)(sa % sb) : (uint32_t)(sa / sb);
    }
    return is_rem ? (a % b) : (a / b);
}

// Pulse start, spin on done, return result.
static uint32_t run_div(uint32_t a, uint32_t b, int f3) {
    dut->operand_a = a; dut->operand_b = b; dut->funct3 = f3;
    dut->start = 1;
    tick();                 // IDLE latches start (or fast-path fires)
    dut->start = 0;
    int guard = 0;
    while (!dut->done && guard < 200) { tick(); guard++; }
    uint32_t r = dut->result;
    tick();                 // let done clear, settle back to IDLE
    return r;
}

static const char* opname(int f3) {
    switch (f3) { case 4: return "DIV  "; case 5: return "DIVU ";
                  case 6: return "REM  "; case 7: return "REMU "; }
    return "?";
}

static void check(uint32_t a, uint32_t b, int f3) {
    uint32_t got = run_div(a, b, f3), exp = golden(a, b, f3);
    checks++;
    if (got != exp) {
        fails++;
        printf("FAIL  %s a=0x%08X b=0x%08X -> 0x%08X  expected 0x%08X\n",
               opname(f3), a, b, got, exp);
    } else {
        printf("pass  %s a=0x%08X b=0x%08X -> 0x%08X\n", opname(f3), a, b, got);
    }
}

// Run all four ops on one operand pair.
static void quad(uint32_t a, uint32_t b) {
    for (int f3 = 4; f3 <= 7; f3++) check(a, b, f3);
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    dut = new Vdivider;

    // reset
    dut->reset = 1; dut->start = 0; dut->clk = 0; dut->eval();
    for (int i = 0; i < 3; i++) tick();
    dut->reset = 0;

    printf("--- basic unsigned/positive ---\n");
    quad(7, 2);            // q3 r1
    quad(100, 7);          // q14 r2
    quad(1, 1);            // q1 r0
    quad(5, 5);            // q1 r0 (equal — NOT the b>a fast path)
    quad(0, 5);            // q0 r0 (b_mag>a_mag fast path)
    quad(255, 16);         // q15 r15

    printf("\n--- large magnitudes ---\n");
    quad(0xFFFFFFFF, 2);          // DIVU 0x7FFFFFFF ; DIV(-1,2)=0
    quad(0xDEADBEEF, 0x1234);     // arbitrary
    quad(0x80000000, 2);          // DIV = 0xC0000000 (-2^30) ; DIVU = 0x40000000

    printf("\n--- signed sign quadrants (DIV/REM) ---\n");
    check(-7, 2, 4); check(-7, 2, 6);     // -3, -1
    check(7, -2, 4); check(7, -2, 6);     // -3,  1
    check(-7, -2, 4); check(-7, -2, 6);   //  3, -1
    check(-3, 5, 4); check(-3, 5, 6);     //  0, -3 (b_mag>a_mag, negative dividend)

    printf("\n--- fast paths: divide-by-zero ---\n");
    quad(5, 0);            // q all-ones, r=5 (all four ops)
    quad(0, 0);            // q all-ones, r=0
    quad(0x80000000, 0);   // q all-ones, r=0x80000000

    printf("\n--- fast paths: signed overflow INT_MIN / -1 ---\n");
    check(0x80000000, 0xFFFFFFFF, 4);   // DIV  -> 0x80000000
    check(0x80000000, 0xFFFFFFFF, 6);   // REM  -> 0
    check(0x80000000, 0xFFFFFFFF, 5);   // DIVU -> 0 (2^31 / (2^32-1)), NOT overflow (unsigned)
    check(0x80000000, 0xFFFFFFFF, 7);   // REMU -> 0x80000000

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
