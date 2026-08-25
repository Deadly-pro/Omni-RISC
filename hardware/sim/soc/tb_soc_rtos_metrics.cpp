/**
 * @file tb_soc_rtos_metrics.cpp
 * @brief Verilator testbench: FreeRTOS scheduler metrics on soc_top.
 *
 * DUT: soc_top. Stages firmware/rtos_metrics.hex as program.hex (run_sim.sh).
 *
 * Firmware measures and prints:
 *   [MET] LAT min=<n> avg=<n> max=<n>   ISR entry latency (cycles)
 *   [MET] SW  min=<n> avg=<n> max=<n>   2-switch taskYIELD round trip
 *   [MET] TICK exp=50000 min=<n> max=<n> n=<n>
 *   [MET] DEV <n>                        worst tick delta deviation
 *   [MET] DONE
 *
 * PASS requires DONE plus plausible numbers (latency < 5k, switch < 20k,
 * tick deltas within 2k of 50000).
 */
#include <cstdio>
#include <cstdlib>
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
static const uint64_t MAX_CYCLES   = 600000000ULL;

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

static long grab_num(const std::string &s, size_t pos)
{
    return strtol(s.c_str() + pos, nullptr, 10);
}

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
    if (open_wave) { dut->trace(tfp, 99); tfp->open("waves/tb_soc_rtos_metrics.vcd"); }
#endif
    UartRxDecoder uart;
    std::string   line;
    bool have_lat = false, have_sw = false, have_tick = false, done = false, failed = false;
    long lat_avg = -1, sw_avg = -1, tick_min = -1, tick_max = -1, dev = -1;
    uint64_t cycle = 0;
    dut->clk = 0; dut->reset = 1; dut->uart_rx = 1;
    printf("[TB] FreeRTOS-on-Omni-RISC scheduler metrics test\n");
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
                if (getenv("TB_DEBUG")) printf("[UART] %s\n", line.c_str());
                if (line.find("[MET] LAT ") == 0) {
                    lat_avg = grab_num(line, line.find("avg=") + 4);
                    printf("[TB] ISR latency avg = %ld cycles (%.3f us)\n",
                           lat_avg, lat_avg / 50.0);
                    have_lat = lat_avg > 0 && lat_avg < 5000;
                } else if (line.find("[MET] SW") == 0 && line.find("min=") != std::string::npos) {
                    sw_avg = grab_num(line, line.find("avg=") + 4);
                    printf("[TB] 2-switch round trip avg = %ld cycles (%.3f us)\n",
                           sw_avg, sw_avg / 50.0);
                    have_sw = sw_avg > 0 && sw_avg < 20000;
                } else if (line.find("[MET] TICK") == 0) {
                    size_t pmin = line.find("min=");
                    size_t pmax = line.find("max=");
                    tick_min = grab_num(line, pmin + 4);
                    tick_max = grab_num(line, pmax + 4);
                    printf("[TB] tick delta range = [%ld, %ld] (ideal 50000)\n",
                           tick_min, tick_max);
                    have_tick = tick_min > 48000 && tick_max < 52000;
                } else if (line.find("[MET] DONE") == 0) {
                    done = true;
                    break;
                } else if (line.find("ASSERT") != std::string::npos ||
                           line.find("MALLOC FAILED") != std::string::npos ||
                           line.find("STACK OVERFLOW") != std::string::npos ||
                           line.find("SCHEDULER RETURNED") != std::string::npos) {
                    printf("[TB] FAILURE LINE: %s\n", line.c_str());
                    failed = true;
                    break;
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
    printf("[TB] cycles=%llu lat=%ld(%d) sw=%ld(%d) tick=[%ld,%ld](%d) done=%d fail=%d\n",
           (unsigned long long)cycle, lat_avg, have_lat, sw_avg, have_sw,
           tick_min, tick_max, have_tick, done, failed);
    bool pass = !failed && done && have_lat && have_sw && have_tick;
    printf(pass ? "[TB] *** PASS ***  Scheduler metrics measured.\n"
                : "[TB] *** FAIL ***  Metrics run did not complete correctly.\n");
#ifdef VCD_TRACE
    if (open_wave) { tfp->close(); delete tfp; }
#endif
    delete dut;
    return pass ? 0 : 1;
}
