/**
 * @file tb_soc_uart.cpp
 * @brief Verilator C++ testbench: SoC UART boot message verification
 *
 * Tests that the Omni-RISC APU SoC boots correctly and emits the expected
 * boot banner via UART.
 *
 * DUT: soc_top
 *   - input  clk, reset
 *   - output uart_tx
 *   - input  uart_rx (tied high = idle/mark)
 *   - output [7:0] gpio_out
 *
 * Test procedure:
 *   1. Assert reset for a few cycles, then release.
 *   2. Monitor the uart_tx pin, decoding 115200-8N1 serial data.
 *   3. Collect decoded bytes into a string.
 *   4. After receiving the full expected message or hitting a timeout,
 *      compare against the expected boot banner.
 *   5. Print PASS / FAIL.
 *
 * Assumptions:
 *   - System clock: 50 MHz  →  period = 20 ns
 *   - Baud rate:    115200  →  bit period ≈ 434 clock cycles @ 50 MHz
 *   - Frame format: 8-N-1  (1 start bit, 8 data bits LSB-first, 1 stop bit)
 *
 * Build (example):
 *   verilator --cc soc_top.v --exe tb_soc_uart.cpp --trace
 *   make -C obj_dir -f Vsoc_top.mk
 *   ./obj_dir/Vsoc_top
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

// Verilator-generated header (name derived from the top module)
#include "Vsoc_top.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ===========================================================================
// Configuration constants
// ===========================================================================

/** System clock frequency in Hz. */
static const uint64_t SYS_CLK_HZ   = 50000000ULL;

/** UART baud rate. */
static const uint64_t BAUD_RATE     = 115200ULL;

/**
 * Number of system clock cycles per UART bit period.
 * 50 MHz / 115200 ≈ 434 cycles.
 */
static const uint64_t CLKS_PER_BIT  = SYS_CLK_HZ / BAUD_RATE;

/** How many cycles to hold reset asserted. */
static const uint64_t RESET_CYCLES  = 20;

/** Maximum simulation cycles before declaring timeout / FAIL. */
static const uint64_t MAX_CYCLES    = 100000;

/** The boot banner we expect the firmware to emit. */
static const char *EXPECTED_MSG     = "Omni-RISC APU v1.0\n";

// ===========================================================================
// UART receiver (bit-bang decoder)
// ===========================================================================

/**
 * State machine for decoding an asynchronous 8-N-1 serial stream.
 *
 * Call `tick()` once per clock cycle with the current value of uart_tx.
 * When a complete byte has been received, `has_byte` is set and the value
 * is available in `rx_byte`.
 */
class UartRxDecoder {
public:
    /** Possible states of the UART receiver FSM. */
    enum State {
        IDLE,       ///< Waiting for a falling edge (start bit)
        START_BIT,  ///< Sampling the middle of the start bit
        DATA_BITS,  ///< Sampling 8 data bits (LSB first)
        STOP_BIT    ///< Waiting through the stop bit
    };

    State    state;
    uint64_t cycle_counter;   ///< Counts clocks within current bit period
    int      bit_index;       ///< Which data bit we are receiving (0-7)
    uint8_t  shift_reg;       ///< Shift register accumulating the byte
    uint8_t  rx_byte;         ///< Last fully received byte
    bool     has_byte;        ///< Set for one cycle when a byte is ready

    UartRxDecoder()
        : state(IDLE), cycle_counter(0), bit_index(0),
          shift_reg(0), rx_byte(0), has_byte(false) {}

    /**
     * Advance the UART decoder by one system clock cycle.
     *
     * @param tx_pin  Current logic level of the uart_tx output from the DUT.
     *
     * The decoder works as follows:
     *   IDLE      – line is high (mark). A falling edge (0) signals the
     *               start of a frame. We move to START_BIT and begin
     *               counting to the middle of the start bit.
     *   START_BIT – At the midpoint (CLKS_PER_BIT / 2) we verify the line
     *               is still low. If so, begin sampling data bits;
     *               otherwise it was a glitch, return to IDLE.
     *   DATA_BITS – Sample one bit every CLKS_PER_BIT cycles (at midpoint).
     *               Shift in LSB-first. After 8 bits, move to STOP_BIT.
     *   STOP_BIT  – Wait one full bit period (line should be high). Then
     *               latch the byte and return to IDLE.
     */
    void tick(uint8_t tx_pin) {
        has_byte = false;  // clear flag each cycle

        switch (state) {
        case IDLE:
            if (tx_pin == 0) {
                // Falling edge detected – possible start bit
                state = START_BIT;
                cycle_counter = 0;
            }
            break;

        case START_BIT:
            cycle_counter++;
            if (cycle_counter >= CLKS_PER_BIT / 2) {
                // We are at the middle of the start bit
                if (tx_pin == 0) {
                    // Valid start bit – prepare to receive 8 data bits
                    state = DATA_BITS;
                    cycle_counter = 0;
                    bit_index = 0;
                    shift_reg = 0;
                } else {
                    // False start (glitch) – go back to idle
                    state = IDLE;
                }
            }
            break;

        case DATA_BITS:
            cycle_counter++;
            if (cycle_counter >= CLKS_PER_BIT) {
                // Sample at midpoint of this data bit
                cycle_counter = 0;
                // Shift in LSB-first
                shift_reg |= (tx_pin & 1) << bit_index;
                bit_index++;
                if (bit_index >= 8) {
                    state = STOP_BIT;
                }
            }
            break;

        case STOP_BIT:
            cycle_counter++;
            if (cycle_counter >= CLKS_PER_BIT) {
                // Frame complete – latch byte
                rx_byte  = shift_reg;
                has_byte = true;
                state    = IDLE;
                cycle_counter = 0;
            }
            break;
        }
    }
};

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

    // VCD waveform dump for debugging
    VerilatedVcdC *tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);           // trace depth
    tfp->open("tb_soc_uart.vcd");  // output file

    // -----------------------------------------------------------------------
    // Simulation variables
    // -----------------------------------------------------------------------
    uint64_t     cycle = 0;
    UartRxDecoder uart_rx_decoder;
    std::string   received_msg;
    bool          test_passed = false;

    // -----------------------------------------------------------------------
    // Initial pin state
    // -----------------------------------------------------------------------
    dut->clk     = 0;
    dut->reset   = 1;   // assert reset
    dut->uart_rx = 1;   // idle / mark (no host transmission)

    printf("[TB] SoC UART Boot Message Test\n");
    printf("[TB] Expected: \"%s\"\n", EXPECTED_MSG);
    printf("[TB] Baud: %llu, Clk: %llu Hz, Cycles/bit: %llu\n",
           (unsigned long long)BAUD_RATE,
           (unsigned long long)SYS_CLK_HZ,
           (unsigned long long)CLKS_PER_BIT);
    printf("[TB] Timeout: %llu cycles\n", (unsigned long long)MAX_CYCLES);
    printf("[TB] -------------------------------------------\n");

    // -----------------------------------------------------------------------
    // Main simulation loop
    // -----------------------------------------------------------------------
    while (cycle < MAX_CYCLES && !Verilated::gotFinish()) {
        // Toggle clock: rising edge
        dut->clk = 1;
        dut->eval();
        tfp->dump(cycle * 20);  // 20 ns period → dump at pos edge

        // Release reset after RESET_CYCLES
        if (cycle == RESET_CYCLES) {
            dut->reset = 0;
            printf("[TB] Reset released at cycle %llu\n",
                   (unsigned long long)cycle);
        }

        // Feed the current uart_tx pin into our software UART decoder.
        // We decode on the rising edge of clk (when the DUT has settled).
        uart_rx_decoder.tick(dut->uart_tx & 1);

        if (uart_rx_decoder.has_byte) {
            char c = (char)uart_rx_decoder.rx_byte;
            received_msg += c;

            // Print decoded character (escape non-printables)
            if (c == '\n') {
                printf("[UART] '\\n' (0x%02X)\n", (unsigned)uart_rx_decoder.rx_byte);
            } else if (c >= 0x20 && c <= 0x7E) {
                printf("[UART] '%c'  (0x%02X)\n", c, (unsigned)uart_rx_decoder.rx_byte);
            } else {
                printf("[UART] 0x%02X\n", (unsigned)uart_rx_decoder.rx_byte);
            }

            // Early exit: check if we've received the full expected message
            if (received_msg.find(EXPECTED_MSG) != std::string::npos) {
                test_passed = true;
                printf("[TB] Complete message received at cycle %llu\n",
                       (unsigned long long)cycle);
                break;
            }
        }

        // Toggle clock: falling edge
        dut->clk = 0;
        dut->eval();
        tfp->dump(cycle * 20 + 10);  // dump at neg edge

        cycle++;
    }

    // -----------------------------------------------------------------------
    // Results
    // -----------------------------------------------------------------------
    printf("[TB] -------------------------------------------\n");
    printf("[TB] Simulation ended at cycle %llu\n", (unsigned long long)cycle);
    printf("[TB] Received string (%zu bytes): \"", received_msg.size());
    for (size_t i = 0; i < received_msg.size(); i++) {
        char c = received_msg[i];
        if (c == '\n')      printf("\\n");
        else if (c == '\r') printf("\\r");
        else                printf("%c", c);
    }
    printf("\"\n");

    if (test_passed) {
        printf("[TB] *** PASS ***  Boot message matches expected output.\n");
    } else {
        printf("[TB] *** FAIL ***  ");
        if (cycle >= MAX_CYCLES) {
            printf("Timeout after %llu cycles.\n", (unsigned long long)MAX_CYCLES);
        } else {
            printf("Message mismatch or simulation ended early.\n");
        }
        printf("[TB] Expected: \"%s\"\n", EXPECTED_MSG);
    }

    // -----------------------------------------------------------------------
    // Cleanup
    // -----------------------------------------------------------------------
    tfp->close();
    delete tfp;
    delete dut;

    return test_passed ? 0 : 1;
}
