/**
 * @file tb_soc_rtos_q.cpp
 * @brief Verilator testbench: FreeRTOS queue producer/consumer on soc_top.
 *
 * DUT: soc_top. Stages firmware/rtos_q.hex as program.hex (run_sim.sh).
 *
 * Firmware: producer task sends 1..1000 into a queue, consumer checks strict
 * ordering and prints "[Q] QUEUE DONE 1000". PASS requires the DONE line with
 * no ORDER FAIL / ASSERT / MALLOC / OVERFLOW lines.
 *
 * VCD is dumped when --wave is passed.
 */
#include <cstdio>
#include <cstring>
#include <string>
#include "verilated.h"
#include "Vsoc_top.h"
#include "Vsoc_top_soc_top.h"      // public-flat-rw internals (debug)
#include "Vsoc_top___024root.h"
#include "Vsoc_top_cpu_top__Dz1.h"
#include "Vsoc_top_decode_stage.h"
#include "Vsoc_top_regfile.h"

#define VCD_TRACE 1
#ifdef VCD_TRACE
    #include "verilated_vcd_c.h"
#endif

static const uint64_t SYS_CLK_HZ   = 50000000ULL;
static const uint64_t BAUD_RATE    = 115200;
static const uint64_t CLKS_PER_BIT = SYS_CLK_HZ / BAUD_RATE;
static const uint64_t RESET_CYCLES = 20;
static const uint64_t MAX_CYCLES   = getenv("TB_MAX") ? atoll(getenv("TB_MAX")) : 400000000ULL;
static const int      N_ITEMS      = 1000;

class UartRxDecoder {
public:
    enum State { IDLE, START_BIT, DATA_BITS, STOP_BIT } state = IDLE;
    uint64_t cycle_counter = 0;
    int      bit_index     = 0;
    uint8_t  shift_reg     = 0;
    uint8_t  rx_byte       = 0;
    bool     has_byte      = false;
    uint8_t  tx_pin_last   = 1;
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
    if (getenv("TB_VCD")) open_wave = true;
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    Vsoc_top *dut = new Vsoc_top;
#ifdef VCD_TRACE
    VerilatedVcdC *tfp = new VerilatedVcdC;
    if (open_wave) { dut->trace(tfp, 99); tfp->open(VCD_FILE); }
#endif
    UartRxDecoder uart;
    std::string   line;
    int           progress = 0;
    unsigned long done_count = 0;
    bool          failed = false;
    uint64_t      cycle = 0;
    dut->clk = 0; dut->reset = 1; dut->uart_rx = 1;
    printf("[TB] FreeRTOS-on-Omni-RISC queue test (%d items)\n", N_ITEMS);
    while (cycle < MAX_CYCLES && !Verilated::gotFinish()) {
        dut->clk = 1; dut->eval();
#ifdef VCD_TRACE
        if (open_wave && cycle >= 865000 && cycle <= 867000) tfp->dump(cycle * 20);
#endif
        if (cycle == RESET_CYCLES) dut->reset = 0;
        uart.tick(dut->uart_tx & 1);
        static uint32_t pc_hist[64];
        static int      pc_idx = 0;
        uint32_t cur_pc = dut->debug_pc;
#ifdef VERILATOR
        auto *c = dut->rootp->soc_top->u_cpu;
        if (c->redirect_valid &&
            (((c->redirect_target >> 28) != 0) || (c->redirect_target & 3))) {
            printf("[BADREDIR] %llu target=%08x\n", (unsigned long long)cycle,
                   c->redirect_target);
            printf("[STAGE] if_id_pc=%08x if_id_instr=%08x id_ex_pc=%08x\n",
                   c->if_id_pc, c->if_id_instr, c->id_ex_pc);
            printf("[STAGE] fwd_rs1=%08x rs1_addr=%u alu_result=%08x\n",
                   c->u_exec__DOT__fwd1__DOT__fwd_rs1_data,
                   c->id_ex_rs1_addr, c->ex_mem_alu_result);
            printf("[STAGE] wb_rd=%u wb_data=%08x wW=%u\n",
                   c->mem_wb_rd, c->mem_wb_rdata, c->mem_wb_reg_write);

            printf("[RA] reg_file[1]=%08x sp(x2)=%08x s0(x8)=%08x\n",
                   c->u_decode->regfile1->reg_file[1],
                   c->u_decode->regfile1->reg_file[2],
                   c->u_decode->regfile1->reg_file[8]);
            for (int k = 0; k < 64; k++)
                printf("[HIST] pc=%08x\n", pc_hist[(pc_idx + k) & 63]);
            break;
        }
#endif
        if ((cur_pc >> 28) != 0x0 || (cur_pc & 3)) {
            printf("[JUNK] %llu\n", (unsigned long long)cycle);
            for (int k = 0; k < 64; k++)
                printf("[HIST] pc=%08x%s\n", pc_hist[(pc_idx + k) & 63],
                       (pc_hist[(pc_idx + k) & 63] & 3) ? " <-- UNALIGNED" : "");
#ifdef VERILATOR
            // full pipeline state at the fault (public-flat-rw build)
            auto *c = dut->rootp->soc_top->u_cpu;
            printf("[STAGE] if_id_pc=%08x if_id_instr=%08x id_ex_pc=%08x\n",
                   c->if_id_pc, c->if_id_instr, c->id_ex_pc);
            printf("[STAGE] fwd_rs1=%08x rs1_addr=%u alu_result=%08x\n",
                   c->u_exec__DOT__fwd1__DOT__fwd_rs1_data,
                   c->id_ex_rs1_addr, c->ex_mem_alu_result);
            printf("[STAGE] wb_rd=%u wb_data=%08x wW=%u redir=%08x rv=%u\n",
                   c->mem_wb_rd, c->mem_wb_rdata, c->mem_wb_reg_write,
                   c->redirect_target, c->redirect_valid);
#endif
            break;
        }
        pc_hist[pc_idx] = cur_pc;
        pc_idx = (pc_idx + 1) & 63;
        if (getenv("TB_TRACE") && cycle >= 100000 && cycle < 40000000 && cycle % 50000 == 0)
            printf("[PC] %llu pc=%08x\n", (unsigned long long)cycle, dut->debug_pc);
        if (getenv("TB_PINS") && cycle < 200000) {
            static uint8_t last = 1;
            uint8_t cur = dut->uart_tx & 1;
            if (cur != last) { printf("[TXCHG] %llu cur=%u\n", (unsigned long long)cycle, cur); last = cur; }
        }
        if (getenv("TB_BYTES") && uart.state == UartRxDecoder::IDLE && uart.tx_pin_last != dut->uart_tx) {
            printf("[PIN] %llu tx=%u\n", (unsigned long long)cycle, dut->uart_tx & 1);
            uart.tx_pin_last = dut->uart_tx & 1;
        }
        if (uart.has_byte) {
            char c = (char)uart.rx_byte;
            if (getenv("TB_DEBUG")) printf("[BYTE] %02x '%c'\n", c, c >= 32 && c < 127 ? c : '.');
            if (c == '\r') { /* skip */ }
            else if (c == '\n') {
                if (line.find("[Q] QUEUE PROGRESS ") == 0) {
                    progress++;
                    printf("[TB] @%llu cycles: %s\n",
                           (unsigned long long)cycle, line.c_str());
                } else if (line == "[Q] QUEUE DONE " + std::to_string(N_ITEMS)) {
                    done_count = (unsigned long)N_ITEMS;
                    break;
                } else if (line.find("ORDER FAIL") != std::string::npos ||
                           line.find("ASSERT") != std::string::npos ||
                           line.find("MALLOC FAILED") != std::string::npos ||
                           line.find("STACK OVERFLOW") != std::string::npos ||
                           line.find("SCHEDULER RETURNED") != std::string::npos) {
                    printf("[TB] @%llu cycles: FAILURE LINE: %s\n",
                           (unsigned long long)cycle, line.c_str());
                    failed = true;
                    break;
                }
                line.clear();
            } else { line += c; }
        }
        dut->clk = 0; dut->eval();
#ifdef VCD_TRACE
        if (open_wave && cycle >= 865000 && cycle <= 867000) tfp->dump(cycle * 20 + 10);
#endif
        cycle++;
    }
    printf("[TB] cycles=%llu progress_reports=%d done=%lu fail_line=%d\n",
           (unsigned long long)cycle, progress, done_count, failed);
    bool pass = !failed && done_count == (unsigned long)N_ITEMS;
    printf(pass ? "[TB] *** PASS ***  Queue delivered 1000 in-order items.\n"
                : "[TB] *** FAIL ***  Queue test did not complete correctly.\n");
#ifdef VCD_TRACE
    if (open_wave) { tfp->close(); delete tfp; }
#endif
    delete dut;
    return pass ? 0 : 1;
}
