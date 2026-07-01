// =============================================================================
// tb_rob.cpp — Verilator testbench for Omni-RISC Reorder Buffer (ROB)
// =============================================================================
//
// DUT: rob (hardware/rtl/cpu/core/rob.v)
//
// Port map:
//   input         clk, reset
//
//   Allocation port (from dispatch):
//     input         alloc_valid
//     input  [4:0]  alloc_dest_reg   // destination register
//     input  [31:0] alloc_pc         // instruction PC (for debug/exceptions)
//     output [3:0]  alloc_tag        // assigned ROB tag (0–15)
//     output        alloc_ready      // 1 = ROB has space (not full)
//
//   Completion port (from execute):
//     input         complete_valid
//     input  [3:0]  complete_tag     // which ROB entry completed
//     input  [31:0] complete_value   // computed result
//     input         complete_exception // exception during execution
//
//   Commit port (oldest-first, in-order):
//     output        commit_valid     // oldest entry is ready to commit
//     output [4:0]  commit_dest_reg
//     output [31:0] commit_value
//     output        commit_exception
//     input         commit_ack       // pipeline acknowledges commit
//
//   Status:
//     output        full             // ROB is at capacity (16 entries)
//     output        empty            // ROB has no entries
//
// ROB invariants:
//   - Entries are allocated in program order (sequential tags)
//   - Entries are committed in allocation order (in-order commit)
//   - Entries may complete (get a result) in any order
//   - An entry cannot commit until it is complete
//   - 16-entry circular buffer, tags 0–15
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vrob.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// Clock
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

static void tick(Vrob* dut, VerilatedVcdC* tfp) {
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

static void do_reset(Vrob* dut, VerilatedVcdC* tfp) {
    dut->reset            = 1;
    dut->alloc_valid      = 0;
    dut->complete_valid   = 0;
    dut->commit_ack       = 0;
    dut->alloc_dest_reg   = 0;
    dut->alloc_pc         = 0;
    dut->complete_tag     = 0;
    dut->complete_value   = 0;
    dut->complete_exception = 0;
    for (int i = 0; i < 4; i++) tick(dut, tfp);
    dut->reset = 0;
    tick(dut, tfp);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

// Allocate one entry; returns the assigned tag.
static uint8_t rob_alloc(Vrob* dut, VerilatedVcdC* tfp,
                          uint8_t dest_reg, uint32_t pc) {
    dut->alloc_valid    = 1;
    dut->alloc_dest_reg = dest_reg;
    dut->alloc_pc       = pc;
    dut->complete_valid = 0;
    dut->commit_ack     = 0;
    tick(dut, tfp);
    uint8_t tag = dut->alloc_tag;
    dut->alloc_valid = 0;
    return tag;
}

// Complete an entry (deliver result).
static void rob_complete(Vrob* dut, VerilatedVcdC* tfp,
                          uint8_t tag, uint32_t value, bool exception = false) {
    dut->complete_valid     = 1;
    dut->complete_tag       = tag;
    dut->complete_value     = value;
    dut->complete_exception = exception ? 1 : 0;
    dut->alloc_valid        = 0;
    dut->commit_ack         = 0;
    tick(dut, tfp);
    dut->complete_valid = 0;
}

// Try to commit the head entry (send commit_ack).
// Returns true if commit_valid was asserted.
static bool rob_commit(Vrob* dut, VerilatedVcdC* tfp) {
    dut->commit_ack     = 1;
    dut->alloc_valid    = 0;
    dut->complete_valid = 0;
    tick(dut, tfp);
    bool committed = dut->commit_valid;
    dut->commit_ack = 0;
    return committed;
}

// ---------------------------------------------------------------------------
// Test tracking
// ---------------------------------------------------------------------------
static int pass_count = 0;
static int fail_count = 0;

static void check(const char* label, uint32_t expected, uint32_t got) {
    if (expected == got) {
        pass_count++;
        printf("  [PASS] %s\n", label);
    } else {
        fail_count++;
        printf("  [FAIL] %s — expected 0x%X, got 0x%X\n",
               label, expected, got);
    }
}

static void check_bool(const char* label, bool expected, bool got) {
    if (expected == got) {
        pass_count++;
        printf("  [PASS] %s\n", label);
    } else {
        fail_count++;
        printf("  [FAIL] %s — expected %d, got %d\n",
               label, (int)expected, (int)got);
    }
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vrob* dut = new Vrob;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_rob.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC Reorder Buffer Testbench\n");
    printf("============================================================\n\n");

    do_reset(dut, tfp);

    // =========================================================================
    // TEST 1: Empty after reset
    // =========================================================================
    printf("--- Test 1: Initial state after reset ---\n");

    check_bool("empty = 1 after reset", true,  dut->empty);
    check_bool("full  = 0 after reset", false, dut->full);
    check_bool("alloc_ready = 1", true, dut->alloc_ready);
    check_bool("commit_valid = 0 (nothing to commit)", false, dut->commit_valid);

    // =========================================================================
    // TEST 2: Allocate entries, verify sequential tags
    // =========================================================================
    printf("\n--- Test 2: Sequential tag allocation ---\n");

    uint8_t tags[4];
    // Allocate 4 entries: dest_reg=x1..x4, PC=0x100..0x10C
    for (int i = 0; i < 4; i++) {
        tags[i] = rob_alloc(dut, tfp, i + 1, 0x100 + i * 4);
    }

    // Tags should be sequential starting from 0
    check("Tag 0", 0, tags[0]);
    check("Tag 1", 1, tags[1]);
    check("Tag 2", 2, tags[2]);
    check("Tag 3", 3, tags[3]);

    check_bool("empty = 0 after 4 allocations", false, dut->empty);
    check_bool("full  = 0 (only 4/16)",         false, dut->full);

    // =========================================================================
    // TEST 3: Out-of-order completion, in-order commit
    // =========================================================================
    printf("\n--- Test 3: Out-of-order completion → in-order commit ---\n");

    // Complete entries OUT OF ORDER: tag2, tag0, tag3, tag1
    rob_complete(dut, tfp, tags[2], 0xCCCCCCCC);
    rob_complete(dut, tfp, tags[0], 0xAAAAAAAA);
    rob_complete(dut, tfp, tags[3], 0xDDDDDDDD);
    rob_complete(dut, tfp, tags[1], 0xBBBBBBBB);

    // Now commit — should happen IN ALLOCATION ORDER: tag0, tag1, tag2, tag3

    // Commit entry 0 (tag0): dest_reg=x1, value=0xAAAAAAAA
    // First, check that commit_valid is asserted for the head
    tick(dut, tfp);  // Let combinational logic settle
    check_bool("commit_valid before ack", true, dut->commit_valid);
    check("commit #0: dest_reg = x1",  1, dut->commit_dest_reg);
    check("commit #0: value",  0xAAAAAAAA, dut->commit_value);
    check_bool("commit #0: no exception", false, dut->commit_exception);
    rob_commit(dut, tfp);

    // Commit entry 1 (tag1): dest_reg=x2, value=0xBBBBBBBB
    tick(dut, tfp);
    check("commit #1: dest_reg = x2",  2, dut->commit_dest_reg);
    check("commit #1: value",  0xBBBBBBBB, dut->commit_value);
    rob_commit(dut, tfp);

    // Commit entry 2 (tag2): dest_reg=x3, value=0xCCCCCCCC
    tick(dut, tfp);
    check("commit #2: dest_reg = x3",  3, dut->commit_dest_reg);
    check("commit #2: value",  0xCCCCCCCC, dut->commit_value);
    rob_commit(dut, tfp);

    // Commit entry 3 (tag3): dest_reg=x4, value=0xDDDDDDDD
    tick(dut, tfp);
    check("commit #3: dest_reg = x4",  4, dut->commit_dest_reg);
    check("commit #3: value",  0xDDDDDDDD, dut->commit_value);
    rob_commit(dut, tfp);

    tick(dut, tfp);
    check_bool("empty after committing all 4", true, dut->empty);

    // =========================================================================
    // TEST 4: Fill ROB to capacity (16 entries), verify full signal
    // =========================================================================
    printf("\n--- Test 4: Fill to capacity (16 entries) ---\n");

    do_reset(dut, tfp);

    for (int i = 0; i < 16; i++) {
        rob_alloc(dut, tfp, (i % 31) + 1, 0x200 + i * 4);
    }

    check_bool("full = 1 after 16 allocations",  true,  dut->full);
    check_bool("alloc_ready = 0 (ROB full)",      false, dut->alloc_ready);
    check_bool("empty = 0",                       false, dut->empty);

    // =========================================================================
    // TEST 5: Drain ROB, verify empty signal
    // =========================================================================
    printf("\n--- Test 5: Drain all entries ---\n");

    // Complete and commit all 16 entries
    for (int i = 0; i < 16; i++) {
        rob_complete(dut, tfp, i, 0x1000 + i);
    }

    for (int i = 0; i < 16; i++) {
        tick(dut, tfp);
        check_bool("commit_valid during drain", true, dut->commit_valid);
        rob_commit(dut, tfp);
    }

    tick(dut, tfp);
    check_bool("empty = 1 after full drain",  true,  dut->empty);
    check_bool("full  = 0 after full drain",  false, dut->full);
    check_bool("alloc_ready = 1 after drain",  true,  dut->alloc_ready);

    // =========================================================================
    // TEST 6: Exception handling
    // =========================================================================
    printf("\n--- Test 6: Exception on commit ---\n");

    do_reset(dut, tfp);

    // Allocate 3 entries
    uint8_t t0 = rob_alloc(dut, tfp, 1, 0x300);
    uint8_t t1 = rob_alloc(dut, tfp, 2, 0x304);  // This one will have exception
    uint8_t t2 = rob_alloc(dut, tfp, 3, 0x308);

    // Complete: t0 normal, t1 with exception, t2 normal
    rob_complete(dut, tfp, t0, 0x100, false);
    rob_complete(dut, tfp, t1, 0x200, true);   // EXCEPTION
    rob_complete(dut, tfp, t2, 0x300, false);

    // Commit t0 — should be normal
    tick(dut, tfp);
    check_bool("commit t0: no exception", false, dut->commit_exception);
    check("commit t0: value", 0x100, dut->commit_value);
    rob_commit(dut, tfp);

    // Commit t1 — should signal exception
    tick(dut, tfp);
    check_bool("commit t1: EXCEPTION signalled", true, dut->commit_exception);
    check("commit t1: dest_reg = x2", 2, dut->commit_dest_reg);
    rob_commit(dut, tfp);

    // Commit t2 — normal (in a real CPU, the pipeline would flush before
    // reaching here, but the ROB itself should still report it correctly)
    tick(dut, tfp);
    check_bool("commit t2: no exception", false, dut->commit_exception);
    rob_commit(dut, tfp);

    // =========================================================================
    // TEST 7: Flush (reset) clears all entries
    // =========================================================================
    printf("\n--- Test 7: Flush (reset) clears ROB ---\n");

    // Allocate some entries
    rob_alloc(dut, tfp, 10, 0x400);
    rob_alloc(dut, tfp, 11, 0x404);
    rob_alloc(dut, tfp, 12, 0x408);

    check_bool("Not empty before flush", false, dut->empty);

    // Reset
    do_reset(dut, tfp);

    check_bool("empty = 1 after flush",      true,  dut->empty);
    check_bool("full  = 0 after flush",      false, dut->full);
    check_bool("alloc_ready = 1 after flush", true,  dut->alloc_ready);

    // Tags should restart from 0
    uint8_t fresh_tag = rob_alloc(dut, tfp, 1, 0x500);
    check("First tag after flush = 0", 0, fresh_tag);

    // =========================================================================
    // TEST 8: Commit stalls until entry is complete
    // =========================================================================
    printf("\n--- Test 8: Commit stalls on incomplete head ---\n");

    do_reset(dut, tfp);

    // Allocate two entries
    uint8_t ta = rob_alloc(dut, tfp, 5, 0x600);
    uint8_t tb = rob_alloc(dut, tfp, 6, 0x604);

    // Complete only the SECOND entry (tb), leave head (ta) incomplete
    rob_complete(dut, tfp, tb, 0xBBBB);

    // Try to commit — should NOT be valid (head is incomplete)
    tick(dut, tfp);
    check_bool("commit_valid = 0 (head incomplete)", false, dut->commit_valid);

    // Now complete the head
    rob_complete(dut, tfp, ta, 0xAAAA);

    // Commit should now be valid
    tick(dut, tfp);
    check_bool("commit_valid = 1 (head now complete)", true, dut->commit_valid);
    check("commit: value = 0xAAAA (head entry)", 0xAAAA, dut->commit_value);
    rob_commit(dut, tfp);

    // Second entry should now be at head and ready
    tick(dut, tfp);
    check_bool("commit_valid = 1 (second entry)", true, dut->commit_valid);
    check("commit: value = 0xBBBB", 0xBBBB, dut->commit_value);
    rob_commit(dut, tfp);

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
