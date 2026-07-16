// =============================================================================
// tb_pc_gen.cpp — Verilator testbench for Omni-RISC PC Generation
// =============================================================================
//
// DUT: pc_gen (hardware/rtl/cpu/core/pc_gen.v)
//
// Port map (RTL must conform to this — same contract style as tb_regfile):
//   input         clk
//   input         reset            // synchronous: pc <- 32'h0000_0000
//   input         stall            // hold pc this cycle
//   input         redirect_valid   // branch/jump resolved in EX
//   input  [31:0] redirect_target
//   input         trap_valid       // trap/mret — HIGHER priority than redirect
//   input  [31:0] trap_target
//   output [31:0] pc               // current fetch address
//   output [31:0] pc_plus4         // pc + 4
//
// Priority (highest first): reset > trap > redirect > stall > pc+4
//
// Tests:
//   1. Reset drives pc to the reset vector (0x0)
//   2. Sequential fetch: 0, 4, 8, 12, ...
//   3. pc_plus4 tracks pc + 4 at all times
//   4. Redirect loads target next cycle, then increments from there
//   5. Stall holds pc; releasing stall resumes increment
//   6. Redirect DURING stall: redirect wins (stalled instr is being flushed)
//   7. Trap beats redirect when both assert in the same cycle
//   8. Reset mid-run returns to vector
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vpc_gen.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// Clock helpers
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

static void tick(Vpc_gen* dut, VerilatedVcdC* tfp) {
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

static void clear_inputs(Vpc_gen* dut) {
    dut->stall           = 0;
    dut->redirect_valid  = 0;
    dut->redirect_target = 0;
    dut->trap_valid      = 0;
    dut->trap_target     = 0;
}

static void do_reset(Vpc_gen* dut, VerilatedVcdC* tfp) {
    clear_inputs(dut);
    dut->reset = 1;
    for (int i = 0; i < 2; i++) tick(dut, tfp);
    dut->reset = 0;
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

    Vpc_gen* dut = new Vpc_gen;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_pc_gen.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC PC Generation Testbench\n");
    printf("============================================================\n\n");

    // =========================================================================
    // TEST 1: Reset drives pc to the reset vector
    // =========================================================================
    printf("--- Test 1: Reset vector ---\n");
    do_reset(dut, tfp);
    check("pc = 0x0 after reset", 0x00000000, dut->pc);

    // =========================================================================
    // TEST 2 + 3: Sequential fetch, pc_plus4 tracks
    // =========================================================================
    printf("\n--- Test 2/3: Sequential fetch + pc_plus4 ---\n");
    for (uint32_t i = 1; i <= 4; i++) {
        tick(dut, tfp);
        char label[64];
        snprintf(label, sizeof(label), "pc = 0x%X after %u ticks", i * 4, i);
        check(label, i * 4, dut->pc);
        snprintf(label, sizeof(label), "pc_plus4 = 0x%X", i * 4 + 4);
        check(label, i * 4 + 4, dut->pc_plus4);
    }
    // pc is now 0x10

    // =========================================================================
    // TEST 4: Redirect loads target, then increments from there
    // =========================================================================
    printf("\n--- Test 4: Redirect ---\n");
    dut->redirect_valid  = 1;
    dut->redirect_target = 0x00000100;
    tick(dut, tfp);
    dut->redirect_valid = 0;
    check("pc = redirect target 0x100", 0x00000100, dut->pc);

    tick(dut, tfp);
    check("pc = 0x104 (increments after redirect)", 0x00000104, dut->pc);

    // =========================================================================
    // TEST 5: Stall holds pc
    // =========================================================================
    printf("\n--- Test 5: Stall hold ---\n");
    dut->stall = 1;
    tick(dut, tfp);
    check("pc held at 0x104 (stall, 1 cycle)", 0x00000104, dut->pc);
    tick(dut, tfp);
    check("pc held at 0x104 (stall, 2 cycles)", 0x00000104, dut->pc);

    dut->stall = 0;
    tick(dut, tfp);
    check("pc = 0x108 after stall release", 0x00000108, dut->pc);

    // =========================================================================
    // TEST 6: Redirect during stall — redirect wins
    // =========================================================================
    printf("\n--- Test 6: Redirect during stall ---\n");
    dut->stall           = 1;
    dut->redirect_valid  = 1;
    dut->redirect_target = 0x00000200;
    tick(dut, tfp);
    dut->stall          = 0;
    dut->redirect_valid = 0;
    check("pc = 0x200 (redirect beats stall)", 0x00000200, dut->pc);

    // =========================================================================
    // TEST 7: Trap beats redirect
    // =========================================================================
    printf("\n--- Test 7: Trap priority over redirect ---\n");
    dut->trap_valid      = 1;
    dut->trap_target     = 0x00000800;
    dut->redirect_valid  = 1;
    dut->redirect_target = 0x00000300;
    tick(dut, tfp);
    dut->trap_valid     = 0;
    dut->redirect_valid = 0;
    check("pc = 0x800 (trap beats redirect)", 0x00000800, dut->pc);

    tick(dut, tfp);
    check("pc = 0x804 (increments after trap)", 0x00000804, dut->pc);

    // =========================================================================
    // TEST 8: Reset mid-run returns to vector
    // =========================================================================
    printf("\n--- Test 8: Reset mid-run ---\n");
    do_reset(dut, tfp);
    check("pc = 0x0 after mid-run reset", 0x00000000, dut->pc);
    tick(dut, tfp);
    check("pc = 0x4 (counts again after reset)", 0x00000004, dut->pc);

    // =========================================================================
    // Summary
    // =========================================================================
    printf("\n============================================================\n");
    printf("  Results: %d PASSED, %d FAILED\n", pass_count, fail_count);
    printf("  Simulation cycles: %lu\n", (unsigned long)(sim_time / 2));
    printf("============================================================\n");

    tfp->close();
    delete tfp;
    delete dut;

    return (fail_count > 0) ? 1 : 0;
}
