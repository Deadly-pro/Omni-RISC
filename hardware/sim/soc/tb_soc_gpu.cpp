/**
 * @file tb_soc_gpu.cpp
 * @brief Verilator C++ testbench: CPU → GPU vector-add dispatch verification
 *
 * Tests the GPU accelerator dispatch path through the full SoC.
 *
 * The firmware running on the CPU is expected to:
 *   1. Write two test arrays A[] and B[] into BRAM at known addresses.
 *   2. Program the GPU command registers to launch a vector-add kernel.
 *   3. Poll the GPU status register until the kernel completes.
 *   4. Read back the result array C[] from BRAM.
 *   5. Verify C[i] == A[i] + B[i] for all elements.
 *   6. Print "PASS" or "FAIL" via UART.
 *
 * This testbench simply:
 *   - Boots the SoC.
 *   - Decodes UART output (115200-8N1 at 50 MHz).
 *   - Looks for "PASS" or "FAIL" in the UART stream.
 *   - Declares test result accordingly.
 *   - Times out after 1 000 000 cycles.
 *
 * DUT: soc_top
 *   - input  clk, reset
 *   - output uart_tx
 *   - input  uart_rx (tied high = idle/mark)
 *   - output [7:0] gpio_out
 *
 * Build (example):
 *   verilator --cc soc_top.v --exe tb_soc_gpu.cpp --trace
 *   make -C obj_dir -f Vsoc_top.mk
 *   ./obj_dir/Vsoc_top
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include "Vsoc_top.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ===========================================================================
// Configuration
// ===========================================================================

static const uint64_t SYS_CLK_HZ   = 50000000ULL;
static const uint64_t BAUD_RATE     = 115200ULL;
static const uint64_t CLKS_PER_BIT  = SYS_CLK_HZ / BAUD_RATE;
static const uint64_t RESET_CYCLES  = 20;
static const uint64_t MAX_CYCLES    = 1000000;

// ===========================================================================
// Reusable UART RX decoder (same as tb_soc_uart.cpp)
// ===========================================================================

/**
 * Bit-bang UART receiver FSM.
 * See tb_soc_uart.cpp for detailed documentation.
 */
class UartRxDecoder {
public:
    enum State { IDLE, START_BIT, DATA_BITS, STOP_BIT };

    State    state;
    uint64_t cycle_counter;
    int      bit_index;
    uint8_t  shift_reg;
    uint8_t  rx_byte;
    bool     has_byte;

    UartRxDecoder()
        : state(IDLE), cycle_counter(0), bit_index(0),
          shift_reg(0), rx_byte(0), has_byte(false) {}

    void tick(uint8_t tx_pin) {
        has_byte = false;
        switch (state) {
        case IDLE:
            if (tx_pin == 0) { state = START_BIT; cycle_counter = 0; }
            break;
        case START_BIT:
            cycle_counter++;
            if (cycle_counter >= CLKS_PER_BIT / 2) {
                if (tx_pin == 0) {
                    state = DATA_BITS; cycle_counter = 0;
                    bit_index = 0; shift_reg = 0;
                } else {
                    state = IDLE;
                }
            }
            break;
        case DATA_BITS:
            cycle_counter++;
            if (cycle_counter >= CLKS_PER_BIT) {
                cycle_counter = 0;
                shift_reg |= (tx_pin & 1) << bit_index;
                bit_index++;
                if (bit_index >= 8) state = STOP_BIT;
            }
            break;
        case STOP_BIT:
            cycle_counter++;
            if (cycle_counter >= CLKS_PER_BIT) {
                rx_byte = shift_reg; has_byte = true;
                state = IDLE; cycle_counter = 0;
            }
            break;
        }
    }
};

// ===========================================================================
// Main testbench
// ===========================================================================

int main(int argc, char **argv) {
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);

    Vsoc_top *dut = new Vsoc_top;

    VerilatedVcdC *tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("tb_soc_gpu.vcd");

    // -----------------------------------------------------------------------
    // Simulation variables
    // -----------------------------------------------------------------------
    uint64_t      cycle = 0;
    UartRxDecoder uart_rx_decoder;
    std::string   received_msg;

    // Track whether the firmware reported PASS or FAIL.
    bool fw_pass = false;
    bool fw_fail = false;

    // -----------------------------------------------------------------------
    // Initial pin state
    // -----------------------------------------------------------------------
    dut->clk     = 0;
    dut->reset   = 1;
    dut->uart_rx = 1;  // idle

    printf("[TB] SoC GPU Dispatch Test (Vector Add)\n");
    printf("[TB] Monitoring UART for firmware PASS/FAIL\n");
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

        // Decode UART
        uart_rx_decoder.tick(dut->uart_tx & 1);

        if (uart_rx_decoder.has_byte) {
            char c = (char)uart_rx_decoder.rx_byte;
            received_msg += c;

            // Live print
            if (c == '\n') {
                printf("[UART] '\\n'\n");
            } else if (c >= 0x20 && c <= 0x7E) {
                printf("[UART] '%c'\n", c);
            } else {
                printf("[UART] 0x%02X\n", (unsigned)uart_rx_decoder.rx_byte);
            }

            // Check for firmware verdict in the accumulated string.
            // We look for "PASS" or "FAIL" as substrings.
            if (received_msg.find("PASS") != std::string::npos) {
                fw_pass = true;
                printf("[TB] Firmware reported PASS at cycle %llu\n",
                       (unsigned long long)cycle);
                break;
            }
            if (received_msg.find("FAIL") != std::string::npos) {
                fw_fail = true;
                printf("[TB] Firmware reported FAIL at cycle %llu\n",
                       (unsigned long long)cycle);
                break;
            }
        }

        // Falling edge
        dut->clk = 0;
        dut->eval();
        tfp->dump(cycle * 20 + 10);

        cycle++;
    }

    // -----------------------------------------------------------------------
    // Results
    // -----------------------------------------------------------------------
    printf("[TB] -------------------------------------------\n");
    printf("[TB] Simulation ended at cycle %llu\n", (unsigned long long)cycle);
    printf("[TB] Full UART output (%zu bytes): \"", received_msg.size());
    for (size_t i = 0; i < received_msg.size(); i++) {
        char c = received_msg[i];
        if (c == '\n')      printf("\\n");
        else if (c == '\r') printf("\\r");
        else                printf("%c", c);
    }
    printf("\"\n");

    if (fw_pass) {
        printf("[TB] *** PASS ***  GPU vector-add kernel completed "
               "successfully.\n");
    } else if (fw_fail) {
        printf("[TB] *** FAIL ***  Firmware self-check reported FAIL.\n");
    } else {
        printf("[TB] *** FAIL ***  Timeout — no PASS/FAIL received from "
               "firmware after %llu cycles.\n",
               (unsigned long long)MAX_CYCLES);
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    tfp->close();
    delete tfp;
    delete dut;

    return fw_pass ? 0 : 1;
}
