/**
 * @file tb_soc_timer.cpp
 * @brief Verilator C++ testbench: SoC timer interrupt verification
 *
 * Tests that the Omni-RISC APU SoC timer interrupt works correctly.
 * The firmware is expected to:
 *   1. Configure mtime / mtimecmp to generate periodic interrupts.
 *   2. In the timer interrupt handler, toggle gpio_out[0].
 *
 * DUT: soc_top
 *   - input  clk, reset
 *   - output uart_tx
 *   - input  uart_rx (tied high = idle/mark)
 *   - output [7:0] gpio_out
 *
 * Test procedure:
 *   1. Assert reset, then release.
 *   2. Monitor gpio_out[0] for logic-level transitions (toggles).
 *   3. After observing 3 toggles, declare PASS.
 *   4. If 500 000 cycles elapse without 3 toggles, declare FAIL.
 *
 * The test also reports the cycle numbers at which each toggle occurs,
 * which lets the designer verify the mtimecmp period is correct.
 *
 * Assumptions:
 *   - System clock: 50 MHz
 *   - Firmware is pre-loaded (via $readmemh or ROM) and boots on reset
 *
 * Build (example):
 *   verilator --cc soc_top.v --exe tb_soc_timer.cpp --trace
 *   make -C obj_dir -f Vsoc_top.mk
 *   ./obj_dir/Vsoc_top
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "Vsoc_top.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ===========================================================================
// Configuration
// ===========================================================================

/** How many cycles to hold reset asserted. */
static const uint64_t RESET_CYCLES  = 20;

/** Maximum simulation cycles before declaring FAIL (timeout). */
static const uint64_t MAX_CYCLES    = 500000;

/** Number of gpio_out[0] toggles required to declare PASS. */
static const int REQUIRED_TOGGLES   = 3;

// ===========================================================================
// Main testbench
// ===========================================================================

int main(int argc, char **argv) {
    // -----------------------------------------------------------------------
    // Verilator initialisation
    // -----------------------------------------------------------------------
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    Vsoc_top *dut = new Vsoc_top;

    VerilatedVcdC *tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("tb_soc_timer.vcd");

    // -----------------------------------------------------------------------
    // Simulation variables
    // -----------------------------------------------------------------------
    uint64_t cycle      = 0;
    int      toggle_cnt = 0;

    // Previous value of gpio_out[0], used to detect edges.
    // Initialised to 0 because that is the expected reset value.
    uint8_t  prev_gpio0 = 0;

    // Record the cycle at which each toggle happens for period analysis.
    std::vector<uint64_t> toggle_cycles;

    bool test_passed = false;

    // -----------------------------------------------------------------------
    // Initial pin state
    // -----------------------------------------------------------------------
    dut->clk     = 0;
    dut->reset   = 1;   // assert reset
    dut->uart_rx = 1;   // idle

    printf("[TB] SoC Timer Interrupt Test\n");
    printf("[TB] Expecting %d toggles on gpio_out[0]\n", REQUIRED_TOGGLES);
    printf("[TB] Timeout: %llu cycles\n", (unsigned long long)MAX_CYCLES);
    printf("[TB] -------------------------------------------\n");

    // -----------------------------------------------------------------------
    // Main simulation loop
    // -----------------------------------------------------------------------
    while (cycle < MAX_CYCLES && !Verilated::gotFinish()) {
        // Rising edge
        dut->clk = 1;
        dut->eval();
        tfp->dump(cycle * 20);

        // Release reset
        if (cycle == RESET_CYCLES) {
            dut->reset = 0;
            printf("[TB] Reset released at cycle %llu\n",
                   (unsigned long long)cycle);
        }

        // Sample gpio_out[0] on rising edge of clk
        uint8_t cur_gpio0 = dut->gpio_out & 0x01;

        // Detect a toggle (rising or falling edge on gpio_out[0])
        if (cycle > RESET_CYCLES && cur_gpio0 != prev_gpio0) {
            toggle_cnt++;
            toggle_cycles.push_back(cycle);
            printf("[TB] gpio_out[0] toggled to %d at cycle %llu "
                   "(toggle #%d)\n",
                   (int)cur_gpio0,
                   (unsigned long long)cycle,
                   toggle_cnt);

            if (toggle_cnt >= REQUIRED_TOGGLES) {
                test_passed = true;
                break;
            }
        }
        prev_gpio0 = cur_gpio0;

        // Falling edge
        dut->clk = 0;
        dut->eval();
        tfp->dump(cycle * 20 + 10);

        cycle++;
    }

    // -----------------------------------------------------------------------
    // Period analysis (if we got multiple toggles)
    // -----------------------------------------------------------------------
    if (toggle_cycles.size() >= 2) {
        printf("[TB] -------------------------------------------\n");
        printf("[TB] Toggle period analysis:\n");
        for (size_t i = 1; i < toggle_cycles.size(); i++) {
            uint64_t delta = toggle_cycles[i] - toggle_cycles[i - 1];
            printf("[TB]   Toggle %zu → %zu: %llu cycles\n",
                   i, i + 1, (unsigned long long)delta);
        }
    }

    // -----------------------------------------------------------------------
    // Results
    // -----------------------------------------------------------------------
    printf("[TB] -------------------------------------------\n");
    printf("[TB] Simulation ended at cycle %llu\n", (unsigned long long)cycle);
    printf("[TB] Total toggles observed: %d / %d required\n",
           toggle_cnt, REQUIRED_TOGGLES);

    if (test_passed) {
        printf("[TB] *** PASS ***  Timer interrupt toggling gpio_out[0] "
               "correctly.\n");
    } else {
        printf("[TB] *** FAIL ***  ");
        if (cycle >= MAX_CYCLES) {
            printf("Timeout after %llu cycles. Only %d toggles seen.\n",
                   (unsigned long long)MAX_CYCLES, toggle_cnt);
        } else {
            printf("Simulation ended early with only %d toggles.\n",
                   toggle_cnt);
        }
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    tfp->close();
    delete tfp;
    delete dut;

    return test_passed ? 0 : 1;
}
