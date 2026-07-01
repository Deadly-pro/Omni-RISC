// =============================================================================
// tb_regfile.cpp — Verilator testbench for Omni-RISC Register File
// =============================================================================
//
// DUT: regfile (hardware/rtl/cpu/core/regfile.v)
//
// Port map:
//   input         clk, reset
//   input  [4:0]  rs1_addr, rs2_addr, rd_addr
//   input  [31:0] rd_data
//   input         rd_write_en
//   output [31:0] rs1_data, rs2_data
//
// Tests:
//   1. x0 always reads 0 even after write attempt
//   2. Write then read back all 32 registers
//   3. Simultaneous read of two different registers
//   4. Write-first behaviour: read-during-write to same address
//   5. Reset clears all registers
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>

#include "Vregfile.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// Clock helpers
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

// Advance one full clock cycle: low → high → low
// We evaluate at both edges so the DUT sees a rising edge.
static void tick(Vregfile* dut, VerilatedVcdC* tfp) {
    // Falling edge
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    // Rising edge
    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

// Assert reset for a few cycles
static void do_reset(Vregfile* dut, VerilatedVcdC* tfp) {
    dut->reset = 1;
    dut->rd_write_en = 0;
    dut->rs1_addr = 0;
    dut->rs2_addr = 0;
    dut->rd_addr = 0;
    dut->rd_data = 0;
    for (int i = 0; i < 4; i++) tick(dut, tfp);
    dut->reset = 0;
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

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vregfile* dut = new Vregfile;

    // VCD trace
    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_regfile.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC Register File Testbench\n");
    printf("============================================================\n\n");

    // ---- Initial reset -----------------------------------------------------
    do_reset(dut, tfp);

    // =========================================================================
    // TEST 1: x0 always reads 0, even after write
    // =========================================================================
    printf("--- Test 1: x0 hardwired to zero ---\n");

    // Attempt to write 0xDEADBEEF to x0
    dut->rd_addr     = 0;
    dut->rd_data     = 0xDEADBEEF;
    dut->rd_write_en = 1;
    tick(dut, tfp);

    // Read x0 back via both ports
    dut->rd_write_en = 0;
    dut->rs1_addr    = 0;
    dut->rs2_addr    = 0;
    tick(dut, tfp);

    check("x0 via rs1 = 0 after write attempt", 0, dut->rs1_data);
    check("x0 via rs2 = 0 after write attempt", 0, dut->rs2_data);

    // =========================================================================
    // TEST 2: Write then read back all 32 registers
    // =========================================================================
    printf("\n--- Test 2: Write/read all 32 registers ---\n");

    // Write unique values to x0–x31
    for (uint32_t r = 0; r < 32; r++) {
        dut->rd_addr     = r;
        dut->rd_data     = 0xA0000000 | (r * 0x111);  // Unique per register
        dut->rd_write_en = 1;
        tick(dut, tfp);
    }
    dut->rd_write_en = 0;

    // Read them back via rs1
    for (uint32_t r = 0; r < 32; r++) {
        dut->rs1_addr = r;
        tick(dut, tfp);

        uint32_t expected;
        if (r == 0) {
            expected = 0;  // x0 is always 0
        } else {
            expected = 0xA0000000 | (r * 0x111);
        }

        char label[64];
        snprintf(label, sizeof(label), "x%u readback", r);
        check(label, expected, dut->rs1_data);
    }

    // =========================================================================
    // TEST 3: Two different reads in same cycle
    // =========================================================================
    printf("\n--- Test 3: Simultaneous dual-port read ---\n");

    // x1 should still contain 0xA0000111, x2 should contain 0xA0000222
    dut->rs1_addr = 1;
    dut->rs2_addr = 2;
    tick(dut, tfp);

    check("rs1 reads x1 = 0xA0000111", 0xA0000111, dut->rs1_data);
    check("rs2 reads x2 = 0xA0000222", 0xA0000222, dut->rs2_data);

    // Different pair
    dut->rs1_addr = 31;
    dut->rs2_addr = 15;
    tick(dut, tfp);

    check("rs1 reads x31 = 0xA0002181", 0xA0000000 | (31 * 0x111), dut->rs1_data);
    check("rs2 reads x15 = 0xA0000FFF", 0xA0000000 | (15 * 0x111), dut->rs2_data);

    // =========================================================================
    // TEST 4: Write-first / bypass — read-during-write to same address
    // =========================================================================
    printf("\n--- Test 4: Write-first (read-during-write same addr) ---\n");

    // Write 0xCAFEBABE to x5 while reading x5 on rs1
    // With write-first behaviour, rs1 should get the NEW value
    dut->rd_addr     = 5;
    dut->rd_data     = 0xCAFEBABE;
    dut->rd_write_en = 1;
    dut->rs1_addr    = 5;
    tick(dut, tfp);

    // NOTE: Whether the DUT forwards in the same cycle or the next depends
    // on the RTL design.  We check for write-first (same-cycle forwarding).
    // If your RTL does NOT forward in the same cycle, the new value will
    // appear one cycle later — adjust the expected value accordingly.
    //
    // Same-cycle forwarding (write-first):
    check("write-first: rs1 reads new x5 = 0xCAFEBABE", 0xCAFEBABE, dut->rs1_data);

    dut->rd_write_en = 0;

    // Also verify x5 persists on a clean read
    dut->rs1_addr = 5;
    tick(dut, tfp);
    check("x5 persists = 0xCAFEBABE", 0xCAFEBABE, dut->rs1_data);

    // =========================================================================
    // TEST 5: Reset clears all registers
    // =========================================================================
    printf("\n--- Test 5: Reset clears all registers ---\n");

    do_reset(dut, tfp);

    // After reset, all registers should read 0
    bool all_zero = true;
    for (uint32_t r = 0; r < 32; r++) {
        dut->rs1_addr = r;
        tick(dut, tfp);
        if (dut->rs1_data != 0) {
            all_zero = false;
            char label[64];
            snprintf(label, sizeof(label), "x%u = 0 after reset", r);
            check(label, 0, dut->rs1_data);
        }
    }
    if (all_zero) {
        pass_count++;
        printf("  [PASS] All 32 registers read 0 after reset\n");
    }

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
