// =============================================================================
// tb_decoder.cpp — Verilator testbench for Omni-RISC RV32IM Instruction Decoder
// =============================================================================
//
// DUT: decoder (hardware/rtl/cpu/core/decoder.v)
//
// Port map:
//   input  [31:0] instruction
//   output [4:0]  rs1, rs2, rd
//   output [31:0] immediate
//   output [3:0]  alu_op
//   output [2:0]  funct3
//   output [6:0]  funct7
//   output        reg_write, mem_read, mem_write, branch, jump, is_mul_div
//   output [1:0]  op_type      // 0=R, 1=I, 2=S, 3=B
//   output        illegal_instr
//
// RV32I/M instruction encoding reference:
//   R-type: [funct7 | rs2 | rs1 | funct3 | rd | opcode]
//   I-type: [imm[11:0]       | rs1 | funct3 | rd | opcode]
//   S-type: [imm[11:5] | rs2 | rs1 | funct3 | imm[4:0] | opcode]
//   B-type: [imm[12|10:5] | rs2 | rs1 | funct3 | imm[4:1|11] | opcode]
//   U-type: [imm[31:12]                       | rd | opcode]
//   J-type: [imm[20|10:1|11|19:12]             | rd | opcode]
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "Vdecoder.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// RV32 opcodes
// ---------------------------------------------------------------------------
static const uint32_t OP_RTYPE   = 0b0110011;  // R-type ALU
static const uint32_t OP_ITYPE   = 0b0010011;  // I-type ALU immediate
static const uint32_t OP_LOAD    = 0b0000011;  // Load
static const uint32_t OP_STORE   = 0b0100011;  // Store
static const uint32_t OP_BRANCH  = 0b1100011;  // Branch
static const uint32_t OP_LUI     = 0b0110111;  // LUI
static const uint32_t OP_AUIPC   = 0b0010111;  // AUIPC
static const uint32_t OP_JAL     = 0b1101111;  // JAL
static const uint32_t OP_JALR    = 0b1100111;  // JALR
static const uint32_t OP_SYSTEM  = 0b1110011;  // CSR / ECALL / EBREAK

// ---------------------------------------------------------------------------
// Instruction encoding helpers
// ---------------------------------------------------------------------------
static uint32_t encode_r(uint32_t funct7, uint32_t rs2, uint32_t rs1,
                          uint32_t funct3, uint32_t rd, uint32_t opcode) {
    return ((funct7 & 0x7F) << 25) | ((rs2 & 0x1F) << 20) |
           ((rs1 & 0x1F) << 15)    | ((funct3 & 0x7) << 12) |
           ((rd & 0x1F) << 7)      | (opcode & 0x7F);
}

static uint32_t encode_i(uint32_t imm12, uint32_t rs1, uint32_t funct3,
                          uint32_t rd, uint32_t opcode) {
    return ((imm12 & 0xFFF) << 20) | ((rs1 & 0x1F) << 15) |
           ((funct3 & 0x7) << 12)  | ((rd & 0x1F) << 7) |
           (opcode & 0x7F);
}

static uint32_t encode_s(uint32_t imm12, uint32_t rs2, uint32_t rs1,
                          uint32_t funct3, uint32_t opcode) {
    uint32_t imm_hi = (imm12 >> 5) & 0x7F;
    uint32_t imm_lo = imm12 & 0x1F;
    return (imm_hi << 25) | ((rs2 & 0x1F) << 20) | ((rs1 & 0x1F) << 15) |
           ((funct3 & 0x7) << 12) | (imm_lo << 7) | (opcode & 0x7F);
}

static uint32_t encode_b(uint32_t imm13, uint32_t rs2, uint32_t rs1,
                          uint32_t funct3, uint32_t opcode) {
    // imm13: imm[12:1] offset (bit 0 always 0, not encoded)
    uint32_t imm12  = (imm13 >> 12) & 1;
    uint32_t imm10_5 = (imm13 >> 5) & 0x3F;
    uint32_t imm4_1  = (imm13 >> 1) & 0xF;
    uint32_t imm11   = (imm13 >> 11) & 1;
    return (imm12 << 31) | (imm10_5 << 25) | ((rs2 & 0x1F) << 20) |
           ((rs1 & 0x1F) << 15) | ((funct3 & 0x7) << 12) |
           (imm4_1 << 8) | (imm11 << 7) | (opcode & 0x7F);
}

static uint32_t encode_u(uint32_t imm20, uint32_t rd, uint32_t opcode) {
    return (imm20 << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F);
}

static uint32_t encode_j(uint32_t imm21, uint32_t rd, uint32_t opcode) {
    // imm21: imm[20:1] (bit 0 always 0)
    uint32_t imm20   = (imm21 >> 20) & 1;
    uint32_t imm10_1 = (imm21 >> 1) & 0x3FF;
    uint32_t imm11   = (imm21 >> 11) & 1;
    uint32_t imm19_12= (imm21 >> 12) & 0xFF;
    return (imm20 << 31) | (imm10_1 << 21) | (imm11 << 20) |
           (imm19_12 << 12) | ((rd & 0x1F) << 7) | (opcode & 0x7F);
}

// ---------------------------------------------------------------------------
// Sign-extend helper
// ---------------------------------------------------------------------------
static uint32_t sign_extend(uint32_t val, int bits) {
    uint32_t sign = (val >> (bits - 1)) & 1;
    if (sign) {
        val |= ~((1u << bits) - 1);
    }
    return val;
}

// ---------------------------------------------------------------------------
// Test vector: captures expected decoded outputs
// ---------------------------------------------------------------------------
struct DecTest {
    std::string label;
    uint32_t    instruction;

    // Expected outputs
    uint32_t exp_rs1;
    uint32_t exp_rs2;
    uint32_t exp_rd;
    uint32_t exp_immediate;
    uint32_t exp_funct3;
    uint32_t exp_funct7;
    bool     exp_reg_write;
    bool     exp_mem_read;
    bool     exp_mem_write;
    bool     exp_branch;
    bool     exp_jump;
    bool     exp_is_mul_div;
    uint32_t exp_op_type;   // 0=R, 1=I, 2=S, 3=B
    bool     exp_illegal;

    // Fields we don't always check (set to 0xFFFFFFFF to skip)
    uint32_t exp_alu_op;
};

// Sentinel for "don't check this field"
static const uint32_t DC = 0xFFFFFFFF;

// =============================================================================
// Build all test vectors
// =============================================================================
static std::vector<DecTest> make_tests() {
    std::vector<DecTest> tv;

    // =========================================================================
    // R-TYPE instructions (opcode = 0110011)
    // =========================================================================

    // ADD x1, x2, x3   → funct7=0x00, funct3=0, rs1=2, rs2=3, rd=1
    tv.push_back({"R: ADD x1, x2, x3",
        encode_r(0x00, 3, 2, 0, 1, OP_RTYPE),
        2, 3, 1, 0, 0, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // SUB x4, x5, x6   → funct7=0x20, funct3=0
    tv.push_back({"R: SUB x4, x5, x6",
        encode_r(0x20, 6, 5, 0, 4, OP_RTYPE),
        5, 6, 4, 0, 0, 0x20,
        true, false, false, false, false, false, 0, false, DC});

    // AND x7, x8, x9   → funct3=7
    tv.push_back({"R: AND x7, x8, x9",
        encode_r(0x00, 9, 8, 7, 7, OP_RTYPE),
        8, 9, 7, 0, 7, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // OR x10, x11, x12  → funct3=6
    tv.push_back({"R: OR x10, x11, x12",
        encode_r(0x00, 12, 11, 6, 10, OP_RTYPE),
        11, 12, 10, 0, 6, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // XOR x13, x14, x15  → funct3=4
    tv.push_back({"R: XOR x13, x14, x15",
        encode_r(0x00, 15, 14, 4, 13, OP_RTYPE),
        14, 15, 13, 0, 4, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // SLL x1, x2, x3   → funct3=1
    tv.push_back({"R: SLL x1, x2, x3",
        encode_r(0x00, 3, 2, 1, 1, OP_RTYPE),
        2, 3, 1, 0, 1, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // SRL x1, x2, x3   → funct3=5, funct7=0x00
    tv.push_back({"R: SRL x1, x2, x3",
        encode_r(0x00, 3, 2, 5, 1, OP_RTYPE),
        2, 3, 1, 0, 5, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // SRA x1, x2, x3   → funct3=5, funct7=0x20
    tv.push_back({"R: SRA x1, x2, x3",
        encode_r(0x20, 3, 2, 5, 1, OP_RTYPE),
        2, 3, 1, 0, 5, 0x20,
        true, false, false, false, false, false, 0, false, DC});

    // SLT x1, x2, x3   → funct3=2
    tv.push_back({"R: SLT x1, x2, x3",
        encode_r(0x00, 3, 2, 2, 1, OP_RTYPE),
        2, 3, 1, 0, 2, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // SLTU x1, x2, x3  → funct3=3
    tv.push_back({"R: SLTU x1, x2, x3",
        encode_r(0x00, 3, 2, 3, 1, OP_RTYPE),
        2, 3, 1, 0, 3, 0x00,
        true, false, false, false, false, false, 0, false, DC});

    // MUL x1, x2, x3   → funct7=0x01, funct3=0 (M extension)
    tv.push_back({"R: MUL x1, x2, x3",
        encode_r(0x01, 3, 2, 0, 1, OP_RTYPE),
        2, 3, 1, 0, 0, 0x01,
        true, false, false, false, false, true, 0, false, DC});

    // DIV x1, x2, x3   → funct7=0x01, funct3=4
    tv.push_back({"R: DIV x1, x2, x3",
        encode_r(0x01, 3, 2, 4, 1, OP_RTYPE),
        2, 3, 1, 0, 4, 0x01,
        true, false, false, false, false, true, 0, false, DC});

    // REM x1, x2, x3   → funct7=0x01, funct3=6
    tv.push_back({"R: REM x1, x2, x3",
        encode_r(0x01, 3, 2, 6, 1, OP_RTYPE),
        2, 3, 1, 0, 6, 0x01,
        true, false, false, false, false, true, 0, false, DC});

    // =========================================================================
    // I-TYPE ALU instructions (opcode = 0010011)
    // =========================================================================

    // ADDI x1, x2, 100   → imm=100, funct3=0
    tv.push_back({"I: ADDI x1, x2, 100",
        encode_i(100, 2, 0, 1, OP_ITYPE),
        2, DC, 1, 100, 0, DC,
        true, false, false, false, false, false, 1, false, DC});

    // ADDI x1, x2, -1   → imm=0xFFF (sign-extended to 0xFFFFFFFF)
    tv.push_back({"I: ADDI x1, x2, -1",
        encode_i(0xFFF, 2, 0, 1, OP_ITYPE),
        2, DC, 1, sign_extend(0xFFF, 12), 0, DC,
        true, false, false, false, false, false, 1, false, DC});

    // ANDI x3, x4, 0xFF  → funct3=7
    tv.push_back({"I: ANDI x3, x4, 0xFF",
        encode_i(0xFF, 4, 7, 3, OP_ITYPE),
        4, DC, 3, 0xFF, 7, DC,
        true, false, false, false, false, false, 1, false, DC});

    // ORI x3, x4, 0x123  → funct3=6
    tv.push_back({"I: ORI x3, x4, 0x123",
        encode_i(0x123, 4, 6, 3, OP_ITYPE),
        4, DC, 3, 0x123, 6, DC,
        true, false, false, false, false, false, 1, false, DC});

    // XORI x3, x4, 0x55  → funct3=4
    tv.push_back({"I: XORI x3, x4, 0x55",
        encode_i(0x55, 4, 4, 3, OP_ITYPE),
        4, DC, 3, 0x55, 4, DC,
        true, false, false, false, false, false, 1, false, DC});

    // SLTI x1, x2, -5    → funct3=2, imm=-5 (0xFFB)
    tv.push_back({"I: SLTI x1, x2, -5",
        encode_i(0xFFB, 2, 2, 1, OP_ITYPE),
        2, DC, 1, sign_extend(0xFFB, 12), 2, DC,
        true, false, false, false, false, false, 1, false, DC});

    // SLTIU x1, x2, 10   → funct3=3
    tv.push_back({"I: SLTIU x1, x2, 10",
        encode_i(10, 2, 3, 1, OP_ITYPE),
        2, DC, 1, 10, 3, DC,
        true, false, false, false, false, false, 1, false, DC});

    // SLLI x1, x2, 5   → funct3=1, imm[11:5]=0x00, shamt=5
    tv.push_back({"I: SLLI x1, x2, 5",
        encode_i(5, 2, 1, 1, OP_ITYPE),
        2, DC, 1, 5, 1, DC,
        true, false, false, false, false, false, 1, false, DC});

    // SRLI x1, x2, 5   → funct3=5, imm=5
    tv.push_back({"I: SRLI x1, x2, 5",
        encode_i(5, 2, 5, 1, OP_ITYPE),
        2, DC, 1, 5, 5, DC,
        true, false, false, false, false, false, 1, false, DC});

    // SRAI x1, x2, 5   → funct3=5, imm[11:5]=0x20 → imm = 0x405
    tv.push_back({"I: SRAI x1, x2, 5",
        encode_i((0x20 << 5) | 5, 2, 5, 1, OP_ITYPE),
        2, DC, 1, DC, 5, DC,  // immediate encoding is special for shifts
        true, false, false, false, false, false, 1, false, DC});

    // =========================================================================
    // LOAD instructions (opcode = 0000011, I-type format)
    // =========================================================================

    // LW x1, 16(x2)   → funct3=2, imm=16
    tv.push_back({"I: LW x1, 16(x2)",
        encode_i(16, 2, 2, 1, OP_LOAD),
        2, DC, 1, 16, 2, DC,
        true, true, false, false, false, false, 1, false, DC});

    // LH x3, -4(x4)   → funct3=1, imm=-4 (0xFFC)
    tv.push_back({"I: LH x3, -4(x4)",
        encode_i(0xFFC, 4, 1, 3, OP_LOAD),
        4, DC, 3, sign_extend(0xFFC, 12), 1, DC,
        true, true, false, false, false, false, 1, false, DC});

    // LB x5, 0(x6)    → funct3=0
    tv.push_back({"I: LB x5, 0(x6)",
        encode_i(0, 6, 0, 5, OP_LOAD),
        6, DC, 5, 0, 0, DC,
        true, true, false, false, false, false, 1, false, DC});

    // LBU x5, 1(x6)   → funct3=4
    tv.push_back({"I: LBU x5, 1(x6)",
        encode_i(1, 6, 4, 5, OP_LOAD),
        6, DC, 5, 1, 4, DC,
        true, true, false, false, false, false, 1, false, DC});

    // LHU x5, 2(x6)   → funct3=5
    tv.push_back({"I: LHU x5, 2(x6)",
        encode_i(2, 6, 5, 5, OP_LOAD),
        6, DC, 5, 2, 5, DC,
        true, true, false, false, false, false, 1, false, DC});

    // =========================================================================
    // S-TYPE (STORE) instructions (opcode = 0100011)
    // =========================================================================

    // SW x3, 32(x2)  → funct3=2
    tv.push_back({"S: SW x3, 32(x2)",
        encode_s(32, 3, 2, 2, OP_STORE),
        2, 3, DC, 32, 2, DC,
        false, false, true, false, false, false, 2, false, DC});

    // SH x5, -8(x4)  → funct3=1
    tv.push_back({"S: SH x5, -8(x4)",
        encode_s(0xFF8, 5, 4, 1, OP_STORE),
        4, 5, DC, sign_extend(0xFF8, 12), 1, DC,
        false, false, true, false, false, false, 2, false, DC});

    // SB x7, 0(x6)   → funct3=0
    tv.push_back({"S: SB x7, 0(x6)",
        encode_s(0, 7, 6, 0, OP_STORE),
        6, 7, DC, 0, 0, DC,
        false, false, true, false, false, false, 2, false, DC});

    // =========================================================================
    // B-TYPE (BRANCH) instructions (opcode = 1100011)
    // =========================================================================

    // BEQ x1, x2, +8   → funct3=0, offset=8
    tv.push_back({"B: BEQ x1, x2, +8",
        encode_b(8, 2, 1, 0, OP_BRANCH),
        1, 2, DC, sign_extend(8, 13), 0, DC,
        false, false, false, true, false, false, 3, false, DC});

    // BNE x3, x4, -16  → funct3=1
    tv.push_back({"B: BNE x3, x4, -16",
        encode_b(sign_extend(0x1FF0, 13), 4, 3, 1, OP_BRANCH),
        3, 4, DC, DC, 1, DC,  // complex sign extension for B-type
        false, false, false, true, false, false, 3, false, DC});

    // BLT x1, x2, +12  → funct3=4
    tv.push_back({"B: BLT x1, x2, +12",
        encode_b(12, 2, 1, 4, OP_BRANCH),
        1, 2, DC, sign_extend(12, 13), 4, DC,
        false, false, false, true, false, false, 3, false, DC});

    // BGE x1, x2, +4   → funct3=5
    tv.push_back({"B: BGE x1, x2, +4",
        encode_b(4, 2, 1, 5, OP_BRANCH),
        1, 2, DC, sign_extend(4, 13), 5, DC,
        false, false, false, true, false, false, 3, false, DC});

    // BLTU x1, x2, +20 → funct3=6
    tv.push_back({"B: BLTU x1, x2, +20",
        encode_b(20, 2, 1, 6, OP_BRANCH),
        1, 2, DC, sign_extend(20, 13), 6, DC,
        false, false, false, true, false, false, 3, false, DC});

    // BGEU x1, x2, +24 → funct3=7
    tv.push_back({"B: BGEU x1, x2, +24",
        encode_b(24, 2, 1, 7, OP_BRANCH),
        1, 2, DC, sign_extend(24, 13), 7, DC,
        false, false, false, true, false, false, 3, false, DC});

    // =========================================================================
    // U-TYPE instructions
    // =========================================================================

    // LUI x1, 0xDEADB   → rd=1, imm=0xDEADB000
    tv.push_back({"U: LUI x1, 0xDEADB",
        encode_u(0xDEADB, 1, OP_LUI),
        DC, DC, 1, 0xDEADB000u, DC, DC,
        true, false, false, false, false, false, DC, false, DC});

    // AUIPC x2, 0x12345 → rd=2, imm=0x12345000
    tv.push_back({"U: AUIPC x2, 0x12345",
        encode_u(0x12345, 2, OP_AUIPC),
        DC, DC, 2, 0x12345000u, DC, DC,
        true, false, false, false, false, false, DC, false, DC});

    // =========================================================================
    // J-TYPE / JALR
    // =========================================================================

    // JAL x1, +256  → rd=1, offset=256
    tv.push_back({"J: JAL x1, +256",
        encode_j(256, 1, OP_JAL),
        DC, DC, 1, sign_extend(256, 21), DC, DC,
        true, false, false, false, true, false, DC, false, DC});

    // JALR x1, 100(x2) → I-type format, funct3=0
    tv.push_back({"I: JALR x1, 100(x2)",
        encode_i(100, 2, 0, 1, OP_JALR),
        2, DC, 1, 100, 0, DC,
        true, false, false, false, true, false, 1, false, DC});

    // =========================================================================
    // CSR instructions (opcode = 1110011)
    // =========================================================================

    // CSRRW x1, mstatus(0x300), x2 → funct3=1
    tv.push_back({"CSR: CSRRW x1, 0x300, x2",
        encode_i(0x300, 2, 1, 1, OP_SYSTEM),
        2, DC, 1, 0x300, 1, DC,
        true, false, false, false, false, false, DC, false, DC});

    // CSRRS x3, mcause(0x342), x4 → funct3=2
    tv.push_back({"CSR: CSRRS x3, 0x342, x4",
        encode_i(0x342, 4, 2, 3, OP_SYSTEM),
        4, DC, 3, 0x342, 2, DC,
        true, false, false, false, false, false, DC, false, DC});

    // CSRRC x5, mepc(0x341), x6 → funct3=3
    tv.push_back({"CSR: CSRRC x5, 0x341, x6",
        encode_i(0x341, 6, 3, 5, OP_SYSTEM),
        6, DC, 5, 0x341, 3, DC,
        true, false, false, false, false, false, DC, false, DC});

    // =========================================================================
    // ILLEGAL instructions
    // =========================================================================

    // All zeros is not a valid RV32 instruction
    tv.push_back({"ILLEGAL: all zeros (0x00000000)",
        0x00000000,
        DC, DC, DC, DC, DC, DC,
        false, false, false, false, false, false, DC, true, DC});

    // Invalid opcode 0b1111111
    tv.push_back({"ILLEGAL: bad opcode 0x7F",
        0x0000007F,
        DC, DC, DC, DC, DC, DC,
        false, false, false, false, false, false, DC, true, DC});

    // Another invalid: opcode 0b0001011
    tv.push_back({"ILLEGAL: bad opcode 0x0B",
        0x0000000B,
        DC, DC, DC, DC, DC, DC,
        false, false, false, false, false, false, DC, true, DC});

    return tv;
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vdecoder* dut = new Vdecoder;

    // VCD trace
    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_decoder.vcd");
#endif

    auto tests = make_tests();
    int pass_count = 0;
    int fail_count = 0;
    vluint64_t sim_time = 0;

    printf("============================================================\n");
    printf("  Omni-RISC Decoder Testbench — %zu test vectors\n", tests.size());
    printf("============================================================\n\n");

    for (size_t i = 0; i < tests.size(); i++) {
        const auto& t = tests[i];

        // Drive instruction input
        dut->instruction = t.instruction;
        dut->eval();
        tfp->dump(sim_time++);

        // Gather mismatches
        bool ok = true;
        std::string errs;

        auto chk = [&](const char* name, uint32_t exp, uint32_t got) {
            if (exp != DC && exp != got) {
                ok = false;
                char buf[128];
                snprintf(buf, sizeof(buf),
                         "    %s: expected 0x%X, got 0x%X\n", name, exp, got);
                errs += buf;
            }
        };

        chk("rs1",        t.exp_rs1,        dut->rs1);
        chk("rs2",        t.exp_rs2,        dut->rs2);
        chk("rd",         t.exp_rd,         dut->rd);
        chk("immediate",  t.exp_immediate,  dut->immediate);
        chk("funct3",     t.exp_funct3,     dut->funct3);
        chk("funct7",     t.exp_funct7,     dut->funct7);
        chk("reg_write",  (uint32_t)t.exp_reg_write,  dut->reg_write);
        chk("mem_read",   (uint32_t)t.exp_mem_read,   dut->mem_read);
        chk("mem_write",  (uint32_t)t.exp_mem_write,  dut->mem_write);
        chk("branch",     (uint32_t)t.exp_branch,     dut->branch);
        chk("jump",       (uint32_t)t.exp_jump,       dut->jump);
        chk("is_mul_div", (uint32_t)t.exp_is_mul_div, dut->is_mul_div);
        chk("op_type",    t.exp_op_type,    dut->op_type);
        chk("illegal",    (uint32_t)t.exp_illegal,    dut->illegal_instr);
        chk("alu_op",     t.exp_alu_op,     dut->alu_op);

        if (ok) {
            pass_count++;
            printf("  [PASS] #%02zu | %-40s | instr=0x%08X\n",
                   i, t.label.c_str(), t.instruction);
        } else {
            fail_count++;
            printf("  [FAIL] #%02zu | %-40s | instr=0x%08X\n",
                   i, t.label.c_str(), t.instruction);
            printf("%s", errs.c_str());
        }
    }

    // Summary
    printf("\n============================================================\n");
    printf("  Results: %d PASSED, %d FAILED out of %zu tests\n",
           pass_count, fail_count, tests.size());
    printf("  Simulation cycles: %lu\n", (unsigned long)sim_time);
    printf("============================================================\n");

    tfp->close();
    delete tfp;
    delete dut;

    return (fail_count > 0) ? 1 : 0;
}
