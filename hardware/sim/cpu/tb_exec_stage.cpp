// =============================================================================
// tb_exec_stage.cpp — Verilator testbench for Omni-RISC EX stage
// =============================================================================
//
// DUT: exec_stage (hardware/rtl/cpu/core/exec_stage.v)
//      instantiates alu + branch_unit internally
//
// Strategy: drive the ID/EX bundle directly (no decoder needed — we ARE the
// pipeline register). Redirect outputs are checked COMBINATIONALLY, before
// any clock edge: fetch consumes them the same cycle the branch sits in EX.
// EX/MEM outputs are checked after the edge.
//
// Tests:
//   1. R-type add — operand B = rs2, result registered, no redirect
//   2. I-type addi w/ negative imm — operand B = imm
//   3. AUIPC — operand A = pc
//   4. Load address — rs1 + imm (op A = rs1 even though mem_read)
//   5. Store — address in alu_result, value in store_data, no redirect
//   6. BEQ taken — redirect_valid SAME CYCLE, target = pc + imm
//   7. BEQ not taken — no redirect
//   8. BLT signed taken (rs1 = -1 < rs2 = 1)
//   9. JAL — unconditional, target = pc + imm, jump/link bits packed
//  10. JALR — target = rs1 + imm with bit 0 CLEARED (odd sum!)
//  11. Reset bubbles EX/MEM control bits
//  12. Stall holds EX/MEM while the bundle changes underneath
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vexec_stage.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// Clock helpers
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

static void tick(Vexec_stage* dut, VerilatedVcdC* tfp) {
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

// Settle combinational logic without a clock edge (redirect checks)
static void settle(Vexec_stage* dut) {
    dut->clk = 0;
    dut->eval();
}

static void clear_bundle(Vexec_stage* dut) {
    dut->stall           = 0;
    dut->id_ex_pc        = 0;
    dut->id_ex_pc_plus4  = 4;
    dut->id_ex_rs1_data  = 0;
    dut->id_ex_rs2_data  = 0;
    dut->id_ex_imm       = 0;
    dut->id_ex_rd        = 0;
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 0;
    dut->id_ex_funct3    = 0;
    dut->id_ex_branch    = 0;
    dut->id_ex_jump      = 0;
    dut->id_ex_reg_write = 0;
    dut->id_ex_mem_read  = 0;
    dut->id_ex_mem_write = 0;
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

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vexec_stage* dut = new Vexec_stage;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_exec_stage.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC Exec Stage Testbench\n");
    printf("============================================================\n\n");

    // Reset once at the start
    clear_bundle(dut);
    dut->reset = 1;
    tick(dut, tfp);
    dut->reset = 0;

    // =====================================================================
    // TEST 1: R-type — add x3, x1, x2  (5 + 7)
    // =====================================================================
    printf("--- Test 1: R-type add (operand B = rs2) ---\n");
    clear_bundle(dut);
    dut->id_ex_rs1_data  = 5;
    dut->id_ex_rs2_data  = 7;
    dut->id_ex_imm       = 0xDEAD;   // poison: must NOT be selected
    dut->id_ex_rd        = 3;
    dut->id_ex_alu_op    = 0;        // ADD
    dut->id_ex_op_type   = 0;        // R
    dut->id_ex_reg_write = 1;
    settle(dut);
    check("no redirect for ALU op", 0, dut->redirect_valid);
    tick(dut, tfp);
    check("alu_result = 12",  12, dut->ex_mem_alu_result);
    check("rd = 3",            3, dut->ex_mem_rd);
    check("reg_write = 1",     1, dut->ex_mem_reg_write);
    check("jump = 0",          0, dut->ex_mem_jump);

    // =====================================================================
    // TEST 2: I-type — addi (10 + (-5)), imm selected not rs2
    // =====================================================================
    printf("\n--- Test 2: addi with negative imm (operand B = imm) ---\n");
    clear_bundle(dut);
    dut->id_ex_rs1_data  = 10;
    dut->id_ex_rs2_data  = 0xDEAD;   // poison
    dut->id_ex_imm       = 0xFFFFFFFB; // -5
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 1;        // I
    dut->id_ex_reg_write = 1;
    tick(dut, tfp);
    check("alu_result = 5", 5, dut->ex_mem_alu_result);

    // =====================================================================
    // TEST 3: AUIPC — operand A = pc
    // =====================================================================
    printf("\n--- Test 3: AUIPC (operand A = pc) ---\n");
    clear_bundle(dut);
    dut->id_ex_pc        = 0x1000;
    dut->id_ex_rs1_data  = 0xDEAD;   // poison: pc must win the mux
    dut->id_ex_imm       = 0x2000;
    dut->id_ex_alu_op    = 11;       // AUIPC
    dut->id_ex_op_type   = 1;
    dut->id_ex_reg_write = 1;
    settle(dut);
    check("no redirect for AUIPC", 0, dut->redirect_valid);
    tick(dut, tfp);
    check("alu_result = pc + imm = 0x3000", 0x3000, dut->ex_mem_alu_result);

    // =====================================================================
    // TEST 4: Load address — lw: rs1 + imm
    // =====================================================================
    printf("\n--- Test 4: load address (rs1 + imm) ---\n");
    clear_bundle(dut);
    dut->id_ex_rs1_data  = 0x1000;
    dut->id_ex_imm       = 8;
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 1;        // I
    dut->id_ex_funct3    = 2;        // LW
    dut->id_ex_reg_write = 1;
    dut->id_ex_mem_read  = 1;
    tick(dut, tfp);
    check("alu_result = 0x1008", 0x1008, dut->ex_mem_alu_result);
    check("mem_read = 1",        1,      dut->ex_mem_mem_read);
    check("funct3 = 2",          2,      dut->ex_mem_funct3);

    // =====================================================================
    // TEST 5: Store — address + store data travel together
    // =====================================================================
    printf("\n--- Test 5: store (addr in result, value in store_data) ---\n");
    clear_bundle(dut);
    dut->id_ex_rs1_data  = 0x2000;
    dut->id_ex_rs2_data  = 0xCAFEBABE;
    dut->id_ex_imm       = 12;
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 2;        // S
    dut->id_ex_mem_write = 1;
    settle(dut);
    check("no redirect for store", 0, dut->redirect_valid);
    tick(dut, tfp);
    check("alu_result = 0x200C (addr)",   0x200C,     dut->ex_mem_alu_result);
    check("store_data = 0xCAFEBABE",      0xCAFEBABE, dut->ex_mem_store_data);
    check("mem_write = 1",                1,          dut->ex_mem_mem_write);
    check("reg_write = 0",                0,          dut->ex_mem_reg_write);

    // =====================================================================
    // TEST 6: BEQ taken — redirect is combinational, target = pc + imm
    // =====================================================================
    printf("\n--- Test 6: beq taken (same-cycle redirect) ---\n");
    clear_bundle(dut);
    dut->id_ex_pc        = 0x100;
    dut->id_ex_rs1_data  = 5;
    dut->id_ex_rs2_data  = 5;
    dut->id_ex_imm       = 0x40;
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 3;        // B
    dut->id_ex_funct3    = 0;        // BEQ
    dut->id_ex_branch    = 1;
    settle(dut);                      // NO clock edge yet!
    check("redirect_valid = 1 (combinational)", 1,     dut->redirect_valid);
    check("redirect_target = 0x140 (pc+imm)",   0x140, dut->redirect_target);
    tick(dut, tfp);
    check("branch writes no register", 0, dut->ex_mem_reg_write);

    // =====================================================================
    // TEST 7: BEQ not taken
    // =====================================================================
    printf("\n--- Test 7: beq not taken ---\n");
    clear_bundle(dut);
    dut->id_ex_rs1_data = 5;
    dut->id_ex_rs2_data = 6;
    dut->id_ex_op_type  = 3;
    dut->id_ex_funct3   = 0;
    dut->id_ex_branch   = 1;
    settle(dut);
    check("redirect_valid = 0", 0, dut->redirect_valid);

    // =====================================================================
    // TEST 8: BLT signed — -1 < 1 taken
    // =====================================================================
    printf("\n--- Test 8: blt signed taken ---\n");
    clear_bundle(dut);
    dut->id_ex_pc       = 0x200;
    dut->id_ex_rs1_data = 0xFFFFFFFF; // -1
    dut->id_ex_rs2_data = 1;
    dut->id_ex_imm      = 0xFFFFFFF0; // -16 (backward branch)
    dut->id_ex_op_type  = 3;
    dut->id_ex_funct3   = 4;          // BLT
    dut->id_ex_branch   = 1;
    settle(dut);
    check("redirect_valid = 1 (signed)",        1,     dut->redirect_valid);
    check("redirect_target = 0x1F0 (backward)", 0x1F0, dut->redirect_target);

    // =====================================================================
    // TEST 9: JAL — unconditional, link bits packed
    // =====================================================================
    printf("\n--- Test 9: jal (target = pc + imm) ---\n");
    clear_bundle(dut);
    dut->id_ex_pc        = 0x114;
    dut->id_ex_pc_plus4  = 0x118;
    dut->id_ex_rs1_data  = 0xDEAD;   // poison: JAL target must use pc
    dut->id_ex_imm       = 16;
    dut->id_ex_rd        = 1;
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 0;        // JAL is op_type 0
    dut->id_ex_jump      = 1;
    dut->id_ex_reg_write = 1;
    settle(dut);
    check("redirect_valid = 1 (unconditional)", 1,     dut->redirect_valid);
    check("redirect_target = 0x124 (pc+imm)",   0x124, dut->redirect_target);
    tick(dut, tfp);
    check("jump bit → WB",       1,     dut->ex_mem_jump);
    check("pc_plus4 = 0x118",    0x118, dut->ex_mem_pc_plus4);
    check("rd = 1, reg_write=1", 1,     dut->ex_mem_rd);
    check("reg_write = 1",       1,     dut->ex_mem_reg_write);

    // =====================================================================
    // TEST 10: JALR — target = rs1 + imm, bit 0 cleared
    // =====================================================================
    printf("\n--- Test 10: jalr (rs1 + imm, bit 0 cleared) ---\n");
    clear_bundle(dut);
    dut->id_ex_pc        = 0xDEAD0000; // poison: JALR must NOT use pc
    dut->id_ex_rs1_data  = 0x2001;
    dut->id_ex_imm       = 2;          // 0x2001 + 2 = 0x2003 (odd!)
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 1;          // JALR is op_type 1
    dut->id_ex_jump      = 1;
    dut->id_ex_reg_write = 1;
    settle(dut);
    check("redirect_valid = 1",                    1,      dut->redirect_valid);
    check("redirect_target = 0x2002 (bit0 clear)", 0x2002, dut->redirect_target);

    // =====================================================================
    // TEST 11: Reset bubbles EX/MEM control
    // =====================================================================
    printf("\n--- Test 11: Reset bubble ---\n");
    dut->reset = 1;
    tick(dut, tfp);
    dut->reset = 0;
    check("reg_write = 0", 0, dut->ex_mem_reg_write);
    check("mem_read = 0",  0, dut->ex_mem_mem_read);
    check("mem_write = 0", 0, dut->ex_mem_mem_write);
    check("jump = 0",      0, dut->ex_mem_jump);

    // =====================================================================
    // TEST 12: Stall holds EX/MEM while bundle changes underneath
    // =====================================================================
    printf("\n--- Test 12: Stall hold ---\n");
    clear_bundle(dut);
    dut->id_ex_rs1_data  = 20;
    dut->id_ex_rs2_data  = 22;
    dut->id_ex_rd        = 7;
    dut->id_ex_alu_op    = 0;
    dut->id_ex_op_type   = 0;
    dut->id_ex_reg_write = 1;
    tick(dut, tfp);                    // EX/MEM = add result 42, rd 7
    dut->id_ex_rs1_data = 999;         // bundle moves on...
    dut->id_ex_rd       = 9;
    dut->stall = 1;
    tick(dut, tfp);
    check("held: alu_result = 42", 42, dut->ex_mem_alu_result);
    check("held: rd = 7",           7, dut->ex_mem_rd);
    dut->stall = 0;
    tick(dut, tfp);
    check("release: rd = 9", 9, dut->ex_mem_rd);

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
