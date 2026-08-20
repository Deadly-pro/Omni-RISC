/**
 * @file tb_soc_rtos.cpp
 * @brief Verilator testbench: run a firmware image on soc_top and decode UART
 *        output to verify the FreeRTOS scheduler.
 *
 * DUT: soc_top. Stages firmware/rtos.hex as program.hex (run_sim.sh).
 *
 * FreeRTOS scheduler verification: two tasks — task A (prio 2) prints
 * "task A tick <n>" every 500ms, task B (prio 1) prints "task B (lower prio)".
 * If both print while the scheduler runs, preemption works.
 *
 * VCD is dumped when --wave is passed.
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
static const uint64_t MAX_CYCLES   = 160000000ULL;
static const int REQUIRED_A_TICKS  = 4;
static const int REQUIRED_B_PRINTS = 2;

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
    if (open_wave) { dut->trace(tfp, 99); tfp->open("waves/tb_soc_rtos.vcd"); }
#endif
    UartRxDecoder uart;
    std::string   line;
    int           a_ticks = 0, b_prints = 0;
    bool          saw_banner = false;
    uint64_t      cycle = 0;
    dut->clk = 0; dut->reset = 1; dut->uart_rx = 1;
    printf("[TB] FreeRTOS-on-Omni-RISC scheduler test\n");
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
                printf("[UART] %s\n", line.c_str());
                if (line == "Omni-RISC APU v1.0") saw_banner = true;
                else if (line.find("task A tick") == 0) a_ticks++;
                else if (line == "task B (lower prio)") b_prints++;
                line.clear();
            } else { line += c; }
            if (saw_banner && a_ticks >= REQUIRED_A_TICKS && b_prints >= REQUIRED_B_PRINTS) break;
        }
        dut->clk = 0; dut->eval();
#ifdef VCD_TRACE
        if (open_wave) tfp->dump(cycle * 20 + 10);
#endif
        cycle++;
    }
    printf("[TB] cycles=%llu banner=%d taskA=%d/%d taskB=%d/%d\n",
           (unsigned long long)cycle, saw_banner?1:0,
           a_ticks, REQUIRED_A_TICKS, b_prints, REQUIRED_B_PRINTS);
    bool pass = saw_banner && a_ticks >= REQUIRED_A_TICKS && b_prints >= REQUIRED_B_PRINTS;
    printf(pass ? "[TB] *** PASS ***  RTOS scheduler running both tasks.\n"
                : "[TB] *** FAIL ***  RTOS scheduler did not demonstrate both tasks.\n");
#ifdef VCD_TRACE
    if (open_wave) { tfp->close(); delete tfp; }
#endif
    delete dut;
    return pass ? 0 : 1;
}
