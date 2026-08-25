/**
 * @file tb_uart_rx.cpp
 * @brief Verilator unit testbench for the uart module's RX path (R2).
 *
 * DUT: uart (mapped explicitly in run_sim.sh). Drives uart_rx bit-banged at
 * exactly 434 cycles/bit and accesses the pbus registers directly, so the
 * FIFO/overrun semantics are tested deterministically (no CPU draining the
 * FIFO concurrently).
 *
 * Cases:
 *   1. idle STATUS: TX busy=0, RX ready=0
 *   2. single byte 'A' -> RXDATA=='A', ready self-clears, pop-on-read
 *   3. empty read does not corrupt FIFO state (next byte still correct)
 *   4. fill 16 bytes back-to-back -> all popped in order, ready clears
 *   5. overrun: 17th byte sets sticky STATUS[2], drops the byte, first
 *      RXDATA read clears the flag
 *   6. glitch immunity: short low pulse while idle produces no byte
 *   7. back-to-back framing: 'AB' with zero inter-frame gap
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include "verilated.h"
#include "Vuart.h"

static const uint64_t CPB = 434;              // 50 MHz / 115200
static const uint64_t RESET_CYCLES = 10;
static uint64_t MAX_CYCLES = getenv("TB_MAX") ? atoll(getenv("TB_MAX")) : 20000000ULL;

static Vuart* dut;
static uint64_t cyc = 0;
static int failures = 0;

static void step(void) {
    dut->clk = 1; dut->eval();
    dut->clk = 0; dut->eval();
    cyc++;
}

static void rx_idle(int bits) {              // hold the line high n bit-times
    for (uint64_t i = 0; i < (uint64_t)bits * CPB && cyc < MAX_CYCLES; i++) {
        dut->uart_rx = 1;
        step();
    }
}

static void rx_bit(int lvl) {
    dut->uart_rx = lvl & 1;
    for (uint64_t i = 0; i < CPB && cyc < MAX_CYCLES; i++)
        step();
}

static void send_byte(uint8_t b) {
    rx_bit(0);                               // start
    for (int k = 0; k < 8; k++)
        rx_bit((b >> k) & 1);                // data, LSB first
    rx_bit(1);                               // stop
}

// one-cycle pbus read of offset. The RTL pops at the read's posedge, so the
// value is sampled combinationally BEFORE that edge — mirroring what the real
// LSU latches (pre-edge rdata, pop happens on the same edge).
static uint32_t read_reg(uint32_t offset) {
    dut->pbus_addr  = 0x40000000u | offset;
    dut->pbus_wen   = 0;
    dut->pbus_read  = 1;
    dut->clk = 0; dut->eval();               // combinational rdata settles
    uint32_t v = dut->pbus_rdata;            // pre-edge value
    dut->clk = 1; dut->eval();               // posedge: pop / flag clear
    dut->clk = 0; dut->eval();
    cyc++;
    dut->pbus_read  = 0;
    dut->pbus_addr  = 0;
    return v;
}

static void write_tx(uint8_t b) {
    dut->pbus_addr  = 0x40000000u;
    dut->pbus_wdata = b;
    dut->pbus_wen   = 0xF;
    step();
    dut->pbus_wen   = 0;
    dut->pbus_addr  = 0;
}

static void expect(bool ok, const char* what) {
    printf("[%s] %s\n", ok ? "ok" : "FAIL", what);
    if (!ok) failures++;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    dut = new Vuart;
    dut->clk = 0; dut->reset = 1;
    dut->uart_rx = 1;
    dut->pbus_addr = 0; dut->pbus_wen = 0; dut->pbus_read = 0; dut->pbus_wdata = 0;
    printf("[TB] uart RX unit test\n");
    while (cyc < RESET_CYCLES) step();
    dut->reset = 0;

    // ---- case 0: TX path still works after the RX additions ----
    {
        write_tx('T');
        expect(dut->uart_tx == 0, "case0: TX start bit drives pin low");
        rx_idle(10);
        expect(dut->uart_tx == 1, "case0: TX returns idle high");
    }

    // ---- case 1: idle status ----
    {
        uint32_t st = read_reg(0x04);
        expect((st & 0x1) == 0, "case1: idle TX busy=0");
        expect((st & 0x2) == 0, "case1: idle RX ready=0");
        expect((st & 0x4) == 0, "case1: idle overrun=0");
    }

    // ---- case 6 first: glitch must not disturb the idle FIFO ----
    {
        dut->uart_rx = 0;
        for (int i = 0; i < 60; i++) step();     // << half a start bit
        dut->uart_rx = 1;
        rx_idle(12);                             // well past any frame time
        uint32_t st = read_reg(0x04);
        expect((st & 0x2) == 0, "case6: glitch produces no byte");
    }

    // ---- case 2: single byte ----
    {
        send_byte('A');
        rx_idle(2);
        uint32_t st = read_reg(0x04);
        expect((st & 0x2) != 0, "case2: ready sets after frame");
        uint32_t d = read_reg(0x08);
        printf("      got=%02x want=41\n", d & 0xFF);
        expect((d & 0xFF) == 'A', "case2: RXDATA=='A'");
        st = read_reg(0x04);
        expect((st & 0x2) == 0, "case2: pop-on-read clears ready");
    }

    // ---- case 3: empty reads leave state intact ----
    {
        (void)read_reg(0x08);
        (void)read_reg(0x08);
        send_byte('B');
        rx_idle(2);
        uint32_t d = read_reg(0x08);
        expect((d & 0xFF) == 'B', "case3: empty reads harmless");
    }

    // ---- case 7: back-to-back framing ----
    {
        send_byte('A');
        send_byte('B');                          // zero inter-frame gap
        rx_idle(2);
        uint32_t a = read_reg(0x08);
        uint32_t b = read_reg(0x08);
        expect((a & 0xFF) == 'A' && (b & 0xFF) == 'B',
               "case7: back-to-back 'AB' in order");
    }

    // ---- case 4: fill the FIFO with 16 bytes ----
    {
        for (int i = 0; i < 16; i++)
            send_byte((uint8_t)('a' + i));
        rx_idle(2);
        bool ok = true;
        for (int i = 0; i < 16; i++) {
            uint32_t d = read_reg(0x08);
            if ((d & 0xFF) != (uint32_t)('a' + i)) {
                printf("      [%d] got=%02x want=%02x\n",
                       i, d & 0xFF, (unsigned)('a' + i));
                ok = false;
            }
        }
        expect(ok, "case4: 16 bytes in order");
        uint32_t st = read_reg(0x04);
        expect((st & 0x2) == 0, "case4: ready clears when drained");
    }

    // ---- case 5: overrun ----
    {
        for (int i = 0; i < 16; i++)             // fill
            send_byte((uint8_t)('A' + i));
        send_byte('Q');                          // 17th: must drop
        rx_idle(2);
        uint32_t st = read_reg(0x04);
        expect((st & 0x4) != 0, "case5: overrun flag sets");
        bool ok = true;
        for (int i = 0; i < 16; i++) {
            uint32_t d = read_reg(0x08);
            if ((d & 0xFF) != (uint32_t)('A' + i)) ok = false;
        }
        expect(ok, "case5: 17th byte dropped, first 16 intact");
        st = read_reg(0x04);
        expect((st & 0x4) == 0, "case5: overrun clears on RXDATA read");
    }

    // ---- recovery sanity: a normal byte still works after all cases ----
    {
        send_byte('Z');
        rx_idle(2);
        uint32_t d = read_reg(0x08);
        expect((d & 0xFF) == 'Z', "recovery: post-test byte ok");
    }

    printf(cyc < MAX_CYCLES ? "[TB] done at %llu cycles\n"
                            : "[TB] TIMEOUT at %llu cycles\n",
           (unsigned long long)cyc);
    printf(failures ? "[TB] *** FAIL *** (%d)\n" : "[TB] *** PASS ***\n",
           failures);
    delete dut;
    return failures ? 1 : 0;
}
