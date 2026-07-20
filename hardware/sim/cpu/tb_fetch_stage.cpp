// =============================================================================
// tb_fetch_stage.cpp — Verilator testbench for Omni-RISC IF stage
// =============================================================================
//
// DUT: fetch_stage (hardware/rtl/cpu/core/fetch_stage.v)
//      instantiates pc_gen + instr_bram internally
//
// Port map (RTL must conform to this):
//   input         clk
//   input         reset
//   input         stall             // hold IF/ID contents; do NOT flush
//   input         redirect_valid    // squash the in-flight fetch
//   input  [31:0] redirect_target
//   input         trap_valid        // squash, higher priority target
//   input  [31:0] trap_target
//   output [31:0] if_id_pc          // PC of if_id_instr (in sync!)
//   output [31:0] if_id_pc_plus4    // if_id_pc + 4
//   output [31:0] if_id_instr       // NOP (0x00000013) when squashed
//
// Test program: the TB writes program.hex itself before construction.
//   mem[i] = (i << 20) | 0x13   — i.e. "addi x0, x0, i"
// so the instruction at PC P must be ((P>>2) << 20) | 0x13. Any pc/instr
// desync shows up as an off-by-one immediate.
//
// Tests:
//   1. During/just after reset the IF/ID instr is the NOP, not garbage
//   2. Sequential fetch: instr matches its own if_id_pc for 4 instructions
//   3. pc_plus4 tracks if_id_pc + 4 (JAL link correctness)
//   4. Redirect: the in-flight dead-path fetch is squashed to NOP,
//      then the target instruction arrives with the target PC
//   5. Stall: IF/ID holds the SAME instruction (not a NOP!) for 2 cycles,
//      then resumes
//   6. Trap: squashes like redirect, fetches from trap_target
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vfetch_stage.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

static const uint32_t NOP = 0x00000013;

// Instruction planted at word index i by the generated hex file
static uint32_t instr_at(uint32_t pc) {
    return ((pc >> 2) << 20) | 0x13;
}

// ---------------------------------------------------------------------------
// Clock helpers
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

static void tick(Vfetch_stage* dut, VerilatedVcdC* tfp) {
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

static void clear_inputs(Vfetch_stage* dut) {
    dut->stall           = 0;
    dut->redirect_valid  = 0;
    dut->redirect_target = 0;
    dut->trap_valid      = 0;
    dut->trap_target     = 0;
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

// Check the full IF/ID bundle is internally consistent for a given PC
static void check_bundle(Vfetch_stage* dut, uint32_t pc, const char* what) {
    char label[96];
    snprintf(label, sizeof(label), "%s: if_id_pc = 0x%X", what, pc);
    check(label, pc, dut->if_id_pc);
    snprintf(label, sizeof(label), "%s: if_id_pc_plus4 = 0x%X", what, pc + 4);
    check(label, pc + 4, dut->if_id_pc_plus4);
    snprintf(label, sizeof(label), "%s: instr matches its PC", what);
    check(label, instr_at(pc), dut->if_id_instr);
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    // -------------------------------------------------------------------
    // Generate the test program BEFORE constructing the DUT ($readmemh
    // runs at init). mem[i] = (i<<20)|0x13 — self-identifying instructions.
    // -------------------------------------------------------------------
    {
        FILE* f = fopen("program.hex", "w");
        if (!f) {
            fprintf(stderr, "ERROR: cannot write program.hex\n");
            return 2;
        }
        for (uint32_t i = 0; i < 64; i++)
            fprintf(f, "%08X\n", (i << 20) | 0x13);
        fclose(f);
    }

    Vfetch_stage* dut = new Vfetch_stage;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_fetch_stage.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC Fetch Stage Testbench\n");
    printf("============================================================\n\n");

    // =====================================================================
    // TEST 1: Reset — IF/ID must present a NOP, never garbage / all-zeros
    // =====================================================================
    printf("--- Test 1: Reset squashes to NOP ---\n");
    dut->reset = 1;
    clear_inputs(dut);
    tick(dut, tfp);
    check("instr = NOP during reset", NOP, dut->if_id_instr);
    tick(dut, tfp);
    dut->reset = 0;

    // =====================================================================
    // TEST 2/3: Sequential fetch — instr / pc / pc+4 stay in lockstep
    // =====================================================================
    printf("\n--- Test 2/3: Sequential fetch, bundle in sync ---\n");
    tick(dut, tfp);   // first real fetch (pc=0) lands in IF/ID
    check_bundle(dut, 0x00, "seq pc=0x00");
    tick(dut, tfp);
    check_bundle(dut, 0x04, "seq pc=0x04");
    tick(dut, tfp);
    check_bundle(dut, 0x08, "seq pc=0x08");
    tick(dut, tfp);
    check_bundle(dut, 0x0C, "seq pc=0x0C");

    // =====================================================================
    // TEST 4: Redirect — dead-path fetch squashed, target arrives after
    // =====================================================================
    printf("\n--- Test 4: Redirect to 0x40 ---\n");
    dut->redirect_valid  = 1;
    dut->redirect_target = 0x40;
    tick(dut, tfp);
    dut->redirect_valid = 0;
    check("dead-path instr squashed to NOP", NOP, dut->if_id_instr);

    tick(dut, tfp);
    check_bundle(dut, 0x40, "post-redirect");
    tick(dut, tfp);
    check_bundle(dut, 0x44, "post-redirect +4");

    // =====================================================================
    // TEST 5: Stall — IF/ID HOLDS the same instruction, no NOP injection
    // =====================================================================
    printf("\n--- Test 5: Stall holds (does not flush!) ---\n");
    dut->stall = 1;
    tick(dut, tfp);
    check_bundle(dut, 0x44, "stall cycle 1 (held)");
    tick(dut, tfp);
    check_bundle(dut, 0x44, "stall cycle 2 (held)");

    dut->stall = 0;
    tick(dut, tfp);
    check_bundle(dut, 0x48, "resume after stall");

    // =====================================================================
    // TEST 6: Trap — squash + fetch from trap vector
    // =====================================================================
    printf("\n--- Test 6: Trap to 0x80 ---\n");
    dut->trap_valid  = 1;
    dut->trap_target = 0x80;
    tick(dut, tfp);
    dut->trap_valid = 0;
    check("dead-path instr squashed to NOP", NOP, dut->if_id_instr);

    tick(dut, tfp);
    check_bundle(dut, 0x80, "post-trap");

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
