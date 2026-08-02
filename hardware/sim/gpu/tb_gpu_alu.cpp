// =============================================================================
// tb_gpu_alu.cpp — Verilator testbench for the 4-lane SIMT ALU
// =============================================================================
// Tests all ALU ops with different values per lane (lane0 in [31:0]).
// Op encoding: 0=ADD 1=SUB 2=AND 3=OR 4=XOR 5=SLT 6=SLTU 7=SLL 8=SRL 9=SRA 10=MUL
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vgpu_alu.h"
#include "verilated.h"

static int checks = 0, fails = 0;

// result is 4 lanes × 32 bits: at(0)=lane0, at(1)=lane1, at(2)=lane2, at(3)=lane3
// return as __uint128_t (two uint64 in a struct is avoided)
static __uint128_t R(const Vgpu_alu* dut) {
    return (__uint128_t)dut->r.at(0) | ((__uint128_t)dut->r.at(1) << 32) |
           ((__uint128_t)dut->r.at(2) << 64) | ((__uint128_t)dut->r.at(3) << 96);
}

// pack 4 lane values into a __uint128_t
static __uint128_t PKG(uint32_t l0, uint32_t l1, uint32_t l2, uint32_t l3) {
    return (__uint128_t)l0 | ((__uint128_t)l1 << 32) |
           ((__uint128_t)l2 << 64) | ((__uint128_t)l3 << 96);
}

static void check(const char* label, __uint128_t expect, __uint128_t got) {
    checks++;
    if (expect == got) { printf("  pass  %s\n", label); }
    else { fails++; printf("  FAIL  %s — expected 0x%08X%08X, got 0x%08X%08X\n",
           label, (unsigned)(expect>>32), (unsigned)expect, (unsigned)(got>>32), (unsigned)got); }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vgpu_alu* dut = new Vgpu_alu;

    // each at(n) is ONE lane: lane0=2, lane1=4, lane2=8, lane3=0x10
    dut->a.at(0) = 0x00000002u;
    dut->a.at(1) = 0x00000004u;
    dut->a.at(2) = 0x00000008u;
    dut->a.at(3) = 0x00000010u;
    // b: lane0=5, lane1=1, lane2=2, lane3=3
    dut->b.at(0) = 0x00000005u;
    dut->b.at(1) = 0x00000001u;
    dut->b.at(2) = 0x00000002u;
    dut->b.at(3) = 0x00000003u;

    dut->alu_op = 0; dut->eval();
    check("ADD", PKG(7, 5, 0xA, 0x13), R(dut));   // lane0=2+5, lane1=4+1, lane2=8+2, lane3=0x10+3

    dut->alu_op = 1; dut->eval();
    check("SUB", PKG(0xFFFFFFFD, 3, 6, 0xD), R(dut));   // 2-5, 4-1, 8-2, 0x10-3

    dut->alu_op = 2; dut->eval();
    check("AND", PKG(0, 0, 0, 0), R(dut));   // all disjoint bits

    dut->alu_op = 3; dut->eval();
    check("OR",  PKG(7, 5, 0xA, 0x13), R(dut));

    dut->alu_op = 4; dut->eval();
    check("XOR", PKG(7, 5, 0xA, 0x13), R(dut));

    // SLT: a < b signed? lane0: 2 < 5 → 1; others 0
    dut->alu_op = 5; dut->eval();
    check("SLT", PKG(1, 0, 0, 0), R(dut));

    // SLTU: same for positive
    dut->alu_op = 6; dut->eval();
    check("SLTU", PKG(1, 0, 0, 0), R(dut));

    // SLL: a << b[4:0] → lane0: 2<<5=0x40, lane1: 4<<1=8, lane2: 8<<2=0x20, lane3: 0x10<<3=0x80
    dut->alu_op = 7; dut->eval();
    check("SLL", PKG(0x40, 8, 0x20, 0x80), R(dut));

    // SRL: a >> b → lane0: 2>>5=0, lane1: 4>>1=2, lane2: 8>>2=2, lane3: 0x10>>3=2
    dut->alu_op = 8; dut->eval();
    check("SRL", PKG(0, 2, 2, 2), R(dut));

    // SRA on negatives: a lane0=-8, b lane0=1 → -8>>1=-4; other lanes 0>>0=0
    for (int i = 0; i < 4; i++) { dut->a.at(i) = 0; dut->b.at(i) = 0; }
    dut->a.at(0) = 0xFFFFFFF8u;
    dut->b.at(0) = 0x00000001u;
    dut->alu_op = 9; dut->eval();
    check("SRA", PKG(0xFFFFFFFC, 0, 0, 0), R(dut));

    // MUL low: lane0: 2*5=10, lane1: 4*1=4, lane2: 8*2=16, lane3: 0x10*3=0x30
    dut->a.at(0) = 0x00000002u;
    dut->a.at(1) = 0x00000004u;
    dut->a.at(2) = 0x00000008u;
    dut->a.at(3) = 0x00000010u;
    dut->b.at(0) = 0x00000005u;
    dut->b.at(1) = 0x00000001u;
    dut->b.at(2) = 0x00000002u;
    dut->b.at(3) = 0x00000003u;
    dut->alu_op = 10; dut->eval();
    check("MUL", PKG(0xA, 4, 0x10, 0x30), R(dut));

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
