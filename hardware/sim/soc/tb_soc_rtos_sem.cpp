/**
 * @file tb_soc_rtos_sem.cpp
 * @brief Verilator testbench: FreeRTOS semaphore given from a CLINT msip ISR.
 *
 * DUT: soc_top. Stages firmware/rtos_sem.hex as program.hex (run_sim.sh).
 *
 * Firmware: trigger task writes the CLINT msip register three times; the
 * machine software interrupt (mcause 0x80000003) is handled by
 * freertos_risc_v_application_interrupt_handler, which gives a binary
 * semaphore; the blocked waiter task counts 3 and prints "[SEM] DONE".
 *
 * PASS requires exactly 3 "[SEM] GOT n" lines then DONE, with no failure lines.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "verilated.h"
#include "Vsoc_top.h"

#define VCD_TRACE 1
#ifdef VCD_TRACE
    #include "verilated_vcd_c.h"
#endif

static const uint64_t SYS_CLK_HZ   = 50000000ULL;
static const uint64_t BAUD_RATE    = 115200;
static const uint64_t CLKS_PER_BIT = SYS_CLK_HZ / BAUD_RATE;
static const uint64_t RESET_CYCLES = 20;
static const uint64_t MAX_CYCLES   = 400000000ULL;

class UartRxDecoder {
public:
    enum State { IDLE, START_BIT, DATA_BITS, STOP_BIT } state = IDLE;
    uint64_t cycle_counter = 0;
    int      bit_index     = 0;
    uint8_t  shift_reg     = 0;
    uint8_t  rx_byte       = 0;
    bool     has_byte      = false;
    void tick(uint8_t tx_pin) {
        has_byte = false;
        switch (state) {
        case IDLE:
            if (tx_pin == 0) { state = START_BIT; cycle_counter = 0; }
            break;
        case START_BIT:
            cycle_counter++;
            if (cycle_counter >= CLKS_PER_BIT / 2) {
                if (tx_pin == 0) { state = DATA_BITS; cycle_counter = 0; bit_index = 0; shift_reg = 0; }
                else { state = IDLE; }
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
                rx_byte = shift_reg; has_byte = true; state = IDLE; cycle_counter = 0;
            }
            break;
        }
    }
};

int main(int argc, char **argv)
{
    bool open_wave = false;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--wave")) open_wave = true;
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vsoc_top *dut = new Vsoc_top;
#ifdef VCD_TRACE
    VerilatedVcdC *tfp = new VerilatedVcdC;
    if (open_wave) { dut->trace(tfp, 99); tfp->open("waves/tb_soc_rtos_sem.vcd"); }
#endif
    UartRxDecoder uart;
    std::string   line;
    int           got_count = 0;
    bool          done = false, failed = false;
    uint64_t      first_got_cycle = 0;
    uint64_t      cycle = 0;
    dut->clk = 0; dut->reset = 1; dut->uart_rx = 1;
    printf("[TB] FreeRTOS-on-Omni-RISC ISR-semaphore test (3 msip raises)\n");
    while (cycle < MAX_CYCLES && !Verilated::gotFinish()) {
        dut->clk = 1; dut->eval();
#ifdef VCD_TRACE
        if (open_wave) tfp->dump(cycle * 20);
#endif
        if (cycle == RESET_CYCLES) dut->reset = 0;
        uart.tick(dut->uart_tx & 1);
        if (uart.has_byte) {
            char c = (char)uart.rx_byte;
            if (c == '\r') { /* skip */ }
            else if (c == '\n') {
                if (line.find("[SEM] GOT ") == 0) {
                    got_count++;
                    if (got_count == 1) first_got_cycle = cycle;
                    printf("[TB] @%llu cycles: %s\n",
                           (unsigned long long)cycle, line.c_str());
                } else if (line == "[SEM] DONE") {
                    done = true;
                    printf("[TB] @%llu cycles: [SEM] DONE\n",
                           (unsigned long long)cycle);
                    break;
                } else {
                    if (getenv("TB_DEBUG")) printf("[UART] %s\n", line.c_str());
                    if (line.find("ASSERT") != std::string::npos ||
                        line.find("MALLOC FAILED") != std::string::npos ||
                        line.find("STACK OVERFLOW") != std::string::npos ||
                        line.find("SCHEDULER RETURNED") != std::string::npos) {
                        printf("[TB] @%llu cycles: FAILURE LINE: %s\n",
                               (unsigned long long)cycle, line.c_str());
                        failed = true;
                        break;
                    }
                }
                line.clear();
            } else { line += c; }
        }
        dut->clk = 0; dut->eval();
#ifdef VCD_TRACE
        if (open_wave) tfp->dump(cycle * 20 + 10);
#endif
        cycle++;
    }
    printf("[TB] cycles=%llu got=%d done=%d fail_line=%d first_ISR_wakeup@%llu\n",
           (unsigned long long)cycle, got_count, done, failed,
           (unsigned long long)first_got_cycle);
    bool pass = !failed && done && got_count == 3;
    printf(pass ? "[TB] *** PASS ***  Semaphore delivered from ISR 3/3 times.\n"
                : "[TB] *** FAIL ***  ISR-semaphore path did not deliver.\n");
#ifdef VCD_TRACE
    if (open_wave) { tfp->close(); delete tfp; }
#endif
    delete dut;
    return pass ? 0 : 1;
}
