// =============================================================================
// tb_multiplier.cpp — unit TB for the RV32M multiplier (combinational)
// =============================================================================
// Covers all four mul ops and, critically, the sign-boundary cases that the
// naive "$signed(a)*$signed(b)" approach silently gets wrong:
//   - width truncation: a*b on two 32-bit nets is a 32-BIT product; the high
//     word is gone before you slice [63:32]. Operands must be widened first.
//   - signedness contagion: one unsigned operand makes the WHOLE expression
//     unsigned, so MULHSU can't mix $signed(a)*b — it needs explicit extend.
//
// funct3: 000 MUL (low32) · 001 MULH (ss high32) · 010 MULHSU (su high32)
//         011 MULHU (uu high32)
// =============================================================================

#include <cstdio>
#include <cstdint>
#include "Vmultiplier.h"
#include "verilated.h"

static int checks = 0, fails = 0;

// Golden model, straight from the ISA semantics.
static uint32_t golden(uint32_t a, uint32_t b, int f3) {
    switch (f3) {
        case 0: { // MUL: low 32 (signedness irrelevant for low word)
            int64_t p = (int64_t)(int32_t)a * (int64_t)(int32_t)b;
            return (uint32_t)(p & 0xFFFFFFFFu);
        }
        case 1: { // MULH: signed x signed, high 32
            int64_t p = (int64_t)(int32_t)a * (int64_t)(int32_t)b;
            return (uint32_t)((uint64_t)p >> 32);
        }
        case 2: { // MULHSU: signed a x unsigned b, high 32
            int64_t p = (int64_t)(int32_t)a * (int64_t)(uint64_t)(uint32_t)b;
            return (uint32_t)((uint64_t)p >> 32);
        }
        case 3: { // MULHU: unsigned x unsigned, high 32
            uint64_t p = (uint64_t)(uint32_t)a * (uint64_t)(uint32_t)b;
            return (uint32_t)(p >> 32);
        }
    }
    return 0;
}

static const char* opname(int f3) {
    switch (f3) { case 0: return "MUL   "; case 1: return "MULH  ";
                  case 2: return "MULHSU"; case 3: return "MULHU "; }
    return "?";
}

static void check(Vmultiplier* d, uint32_t a, uint32_t b, int f3) {
    d->operand_a = a; d->operand_b = b; d->funct3 = f3;
    d->eval();
    uint32_t got = d->result, exp = golden(a, b, f3);
    checks++;
    if (got != exp) {
        fails++;
        printf("FAIL  %s a=0x%08X b=0x%08X -> 0x%08X  expected 0x%08X\n",
               opname(f3), a, b, got, exp);
    } else {
        printf("pass  %s a=0x%08X b=0x%08X -> 0x%08X\n", opname(f3), a, b, got);
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vmultiplier* d = new Vmultiplier;

    const uint32_t A = 0x00000003, B = 0x00000004;
    const uint32_t NEG1 = 0xFFFFFFFFu, IMIN = 0x80000000u, IMAX = 0x7FFFFFFFu;
    const uint32_t TWO = 0x00000002;

    printf("--- MUL (low word) ---\n");
    check(d, A, B, 0);                 // 3*4 = 12
    check(d, NEG1, NEG1, 0);           // low32(-1 * -1) = 1
    check(d, IMIN, NEG1, 0);           // low32 = 0x80000000
    check(d, 12345, 67890, 0);

    printf("\n--- MULH (signed x signed, high word) ---\n");
    check(d, IMIN, IMIN, 1);           // (-2^31)^2 = 2^62 -> high 0x40000000
    check(d, IMIN, NEG1, 1);           // 2^31       -> high 0x00000000
    check(d, NEG1, NEG1, 1);           // 1          -> high 0
    check(d, IMAX, IMAX, 1);           // high 0x3FFFFFFF
    check(d, NEG1, TWO,  1);           // -2         -> high 0xFFFFFFFF

    printf("\n--- MULHU (unsigned x unsigned, high word) ---\n");
    check(d, NEG1, NEG1, 3);           // 0xFFFFFFFE00000001 -> high 0xFFFFFFFE
    check(d, IMIN, IMIN, 3);           // high 0x40000000
    check(d, NEG1, TWO,  3);           // 0x1FFFFFFFE -> high 0x00000001

    printf("\n--- MULHSU (signed a x unsigned b, high word) ---\n");
    check(d, NEG1, NEG1, 2);           // (-1) * 4294967295 -> high 0xFFFFFFFF
    check(d, IMIN, TWO,  2);           // (-2^31) * 2 = -2^32 -> high 0xFFFFFFFF
    check(d, TWO,  NEG1, 2);           // (+2) * 4294967295   -> high 0x00000001
    check(d, IMAX, NEG1, 2);           // positive a, distinguishes from MULH

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete d;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
