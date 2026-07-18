// =============================================================================
// tb_decode_stage.cpp — Verilator testbench for Omni-RISC ID stage
// =============================================================================
//
// DUT: decode_stage (hardware/rtl/cpu/core/decode_stage.v)
//      instantiates decoder (+imm_gen) + regfile internally
//
// Strategy: preload registers through the WB write port (the same path
// wb_stage will use), then present hand-encoded RV32I instructions on
// if_id_instr and check the full ID/EX bundle one cycle later.
//
// Tests:
//   1. Reset bubbles the control bits
//   2. WB port writes reach the regfile (preload x1, x2)
//   3. R-type:  add  x3, x1, x2   — operands read, control bits, rd
//   4. I-type:  addi x4, x1, -5   — sign-extended immediate, op_type=1
//   5. Load:    lw   x6, 8(x1)    — mem_read, funct3=010
//   6. Store:   sw   x2, 12(x1)   — mem_write, reg_write=0, S-imm,
//                                    rs2_data carries the store value
//   7. Branch:  beq  x1, x2, +16  — branch=1, B-imm, op_type=3
//   8. Jump:    jal  x1, +16      — jump=1, reg_write=1, J-imm, pc_plus4
//   9. x0 reads as zero even after a write attempt
//  10. Flush bubbles a live instruction (control=0)
//  11. Flush wins over stall
//  12. Stall holds the PREVIOUS bundle while if_id changes underneath
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vdecode_stage.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

static const uint32_t NOP = 0x00000013;

// Hand-assembled RV32I (verified against the ISA manual bit layouts)
static const uint32_t ADD_X3_X1_X2   = 0x002081B3; // add  x3, x1, x2
static const uint32_t ADDI_X4_X1_M5  = 0xFFB08213; // addi x4, x1, -5
static const uint32_t LW_X6_8_X1     = 0x0080A303; // lw   x6, 8(x1)
static const uint32_t SW_X2_12_X1    = 0x0020A623; // sw   x2, 12(x1)
static const uint32_t BEQ_X1_X2_16   = 0x00208863; // beq  x1, x2, +16
static const uint32_t JAL_X1_16      = 0x010000EF; // jal  x1, +16
static const uint32_t ADD_X3_X0_X0   = 0x000001B3; // add  x3, x0, x0

static const uint32_t X1_VAL = 0x11111111;
static const uint32_t X2_VAL = 0x22222222;

// ---------------------------------------------------------------------------
// Clock helpers
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

static void tick(Vdecode_stage* dut, VerilatedVcdC* tfp) {
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

static void clear_inputs(Vdecode_stage* dut) {
    dut->stall          = 0;
    dut->flush          = 0;
    dut->if_id_pc       = 0;
    dut->if_id_pc_plus4 = 4;
    dut->if_id_instr    = NOP;
    dut->wb_rd_addr     = 0;
    dut->wb_rd_data     = 0;
    dut->wb_reg_write   = 0;
}

// Preload a register through the WB port (one dedicated cycle, NOP in ID)
static void wb_write(Vdecode_stage* dut, VerilatedVcdC* tfp,
                     uint8_t addr, uint32_t data) {
    dut->if_id_instr  = NOP;
    dut->wb_rd_addr   = addr;
    dut->wb_rd_data   = data;
    dut->wb_reg_write = 1;
    tick(dut, tfp);
    dut->wb_reg_write = 0;
}

// Present an instruction (with a fake pc) and clock it into ID/EX
static void feed(Vdecode_stage* dut, VerilatedVcdC* tfp,
                 uint32_t instr, uint32_t pc) {
    dut->if_id_instr    = instr;
    dut->if_id_pc       = pc;
    dut->if_id_pc_plus4 = pc + 4;
    tick(dut, tfp);
}

// ---------------------------------------------------------------------------
// Test result tracking
// ---------------------------------------------------------------------------
static int pass_count = 0;
static int fail_count = 0;

static void check(const char* label, uint32_t expected, uint32_t got) {
    if (expected == got) {
        pass_count++;
        printf("  [PASS] %s\n", label);
    } else {
        fail_count++;
        printf("  [FAIL] %s — expected 0x%08X, got 0x%08X\n",
               label, expected, got);
    }
}

// Bundle control-bit snapshot: {reg_write, mem_read, mem_write, branch, jump}
static void check_ctrl(Vdecode_stage* dut, const char* what,
                       int rw, int mr, int mw, int br, int jp) {
    char label[96];
    snprintf(label, sizeof(label), "%s: reg_write=%d", what, rw);
    check(label, rw, dut->id_ex_reg_write);
    snprintf(label, sizeof(label), "%s: mem_read=%d", what, mr);
    check(label, mr, dut->id_ex_mem_read);
    snprintf(label, sizeof(label), "%s: mem_write=%d", what, mw);
    check(label, mw, dut->id_ex_mem_write);
    snprintf(label, sizeof(label), "%s: branch=%d", what, br);
    check(label, br, dut->id_ex_branch);
    snprintf(label, sizeof(label), "%s: jump=%d", what, jp);
    check(label, jp, dut->id_ex_jump);
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vdecode_stage* dut = new Vdecode_stage;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_decode_stage.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC Decode Stage Testbench\n");
    printf("============================================================\n\n");

    // =====================================================================
    // TEST 1: Reset bubbles control
    // =====================================================================
    printf("--- Test 1: Reset bubble ---\n");
    clear_inputs(dut);
    dut->reset = 1;
    tick(dut, tfp);
    tick(dut, tfp);
    check_ctrl(dut, "reset", 0, 0, 0, 0, 0);
    dut->reset = 0;

    // =====================================================================
    // TEST 2: Preload x1, x2 via the WB port
    // =====================================================================
    printf("\n--- Test 2: WB-port register preload ---\n");
    wb_write(dut, tfp, 1, X1_VAL);
    wb_write(dut, tfp, 2, X2_VAL);
    // Verify by decoding add x3,x1,x2 below — no backdoor peeking.

    // =====================================================================
    // TEST 3: R-type — add x3, x1, x2
    // =====================================================================
    printf("\n--- Test 3: add x3, x1, x2 ---\n");
    feed(dut, tfp, ADD_X3_X1_X2, 0x100);
    check("rs1_data = x1", X1_VAL, dut->id_ex_rs1_data);
    check("rs2_data = x2", X2_VAL, dut->id_ex_rs2_data);
    check("rd = 3",        3,      dut->id_ex_rd);
    check("rs1_addr = 1",  1,      dut->id_ex_rs1_addr);
    check("rs2_addr = 2",  2,      dut->id_ex_rs2_addr);
    check("alu_op = ADD",  0,      dut->id_ex_alu_op);
    check("op_type = R",   0,      dut->id_ex_op_type);
    check("pc = 0x100",    0x100,  dut->id_ex_pc);
    check_ctrl(dut, "add", 1, 0, 0, 0, 0);

    // =====================================================================
    // TEST 4: I-type — addi x4, x1, -5 (sign extension through the bundle)
    // =====================================================================
    printf("\n--- Test 4: addi x4, x1, -5 ---\n");
    feed(dut, tfp, ADDI_X4_X1_M5, 0x104);
    check("imm = -5 (sext)", 0xFFFFFFFB, dut->id_ex_imm);
    check("rd = 4",          4,          dut->id_ex_rd);
    check("op_type = I",     1,          dut->id_ex_op_type);
    check("rs1_data = x1",   X1_VAL,     dut->id_ex_rs1_data);
    check_ctrl(dut, "addi", 1, 0, 0, 0, 0);

    // =====================================================================
    // TEST 5: Load — lw x6, 8(x1)
    // =====================================================================
    printf("\n--- Test 5: lw x6, 8(x1) ---\n");
    feed(dut, tfp, LW_X6_8_X1, 0x108);
    check("imm = 8",       8, dut->id_ex_imm);
    check("rd = 6",        6, dut->id_ex_rd);
    check("funct3 = 010",  2, dut->id_ex_funct3);
    check_ctrl(dut, "lw", 1, 1, 0, 0, 0);

    // =====================================================================
    // TEST 6: Store — sw x2, 12(x1)
    // =====================================================================
    printf("\n--- Test 6: sw x2, 12(x1) ---\n");
    feed(dut, tfp, SW_X2_12_X1, 0x10C);
    check("imm = 12 (S-format)",  12,     dut->id_ex_imm);
    check("rs2_data = store val", X2_VAL, dut->id_ex_rs2_data);
    check_ctrl(dut, "sw", 0, 0, 1, 0, 0);

    // =====================================================================
    // TEST 7: Branch — beq x1, x2, +16
    // =====================================================================
    printf("\n--- Test 7: beq x1, x2, +16 ---\n");
    feed(dut, tfp, BEQ_X1_X2_16, 0x110);
    check("imm = 16 (B-format)", 16,     dut->id_ex_imm);
    check("op_type = B",         3,      dut->id_ex_op_type);
    check("funct3 = BEQ (000)",  0,      dut->id_ex_funct3);
    check("rs1_data = x1",       X1_VAL, dut->id_ex_rs1_data);
    check("rs2_data = x2",       X2_VAL, dut->id_ex_rs2_data);
    check_ctrl(dut, "beq", 0, 0, 0, 1, 0);

    // =====================================================================
    // TEST 8: Jump — jal x1, +16 (link value rides in pc_plus4)
    // =====================================================================
    printf("\n--- Test 8: jal x1, +16 ---\n");
    feed(dut, tfp, JAL_X1_16, 0x114);
    check("imm = 16 (J-format)", 16,    dut->id_ex_imm);
    check("rd = 1 (link reg)",   1,     dut->id_ex_rd);
    check("pc_plus4 = 0x118",    0x118, dut->id_ex_pc_plus4);
    check_ctrl(dut, "jal", 1, 0, 0, 0, 1);

    // =====================================================================
    // TEST 9: x0 is immune — write attempt, then read through decode
    // =====================================================================
    printf("\n--- Test 9: x0 stays zero ---\n");
    wb_write(dut, tfp, 0, 0xDEADBEEF);
    feed(dut, tfp, ADD_X3_X0_X0, 0x118);
    check("rs1_data = 0 (x0)", 0, dut->id_ex_rs1_data);
    check("rs2_data = 0 (x0)", 0, dut->id_ex_rs2_data);

    // =====================================================================
    // TEST 10: Flush bubbles a live instruction
    // =====================================================================
    printf("\n--- Test 10: Flush bubble ---\n");
    dut->if_id_instr = ADD_X3_X1_X2;   // real instruction in ID...
    dut->flush = 1;                    // ...but EX says it's dead-path
    tick(dut, tfp);
    dut->flush = 0;
    check_ctrl(dut, "flush", 0, 0, 0, 0, 0);

    // =====================================================================
    // TEST 11: Flush wins over stall
    // =====================================================================
    printf("\n--- Test 11: Flush beats stall ---\n");
    feed(dut, tfp, ADD_X3_X1_X2, 0x120);   // load a live bundle first
    dut->flush = 1;
    dut->stall = 1;
    tick(dut, tfp);
    dut->flush = 0;
    dut->stall = 0;
    check_ctrl(dut, "flush+stall", 0, 0, 0, 0, 0);

    // =====================================================================
    // TEST 12: Stall holds the bundle while if_id changes underneath
    // =====================================================================
    printf("\n--- Test 12: Stall hold ---\n");
    feed(dut, tfp, LW_X6_8_X1, 0x124);     // bundle now = lw
    dut->if_id_instr = SW_X2_12_X1;        // fetch moved on...
    dut->stall = 1;
    tick(dut, tfp);
    check("held: rd = 6 (still lw)",   6, dut->id_ex_rd);
    check("held: mem_read = 1",        1, dut->id_ex_mem_read);
    check("held: mem_write = 0",       0, dut->id_ex_mem_write);
    tick(dut, tfp);
    check("held 2 cycles: rd = 6",     6, dut->id_ex_rd);
    dut->stall = 0;
    tick(dut, tfp);
    check("release: mem_write = 1 (sw)", 1, dut->id_ex_mem_write);
    check("release: reg_write = 0",      0, dut->id_ex_reg_write);

    // =====================================================================
    // Summary
    // =====================================================================
    printf("\n============================================================\n");
    printf("  Results: %d PASSED, %d FAILED\n", pass_count, fail_count);
    printf("  Simulation cycles: %lu\n", (unsigned long)(sim_time / 2));
    printf("============================================================\n");

    tfp->close();
    delete tfp;
    delete dut;

    return (fail_count > 0) ? 1 : 0;
}
