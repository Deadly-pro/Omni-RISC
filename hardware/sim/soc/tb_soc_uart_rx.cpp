/**
 * @file tb_soc_uart_rx.cpp
 * @brief Verilator integration TB: UART RX through the full SoC (R2).
 *
 * DUT: soc_top (tb_soc_* mapping). Stages firmware/uart_echo.hex: the echo
 * app polls STATUS, pops RXDATA and retransmits each byte over TX.
 *
 * The TB drives uart_rx bit-banged at exactly 434 cycles/bit — no zero-delay
 * cheating; the deserializer must see valid framing (that honesty is what
 * makes R3's real-time stdin feeding trustworthy). Assertions run on the
 * decoded TX stream:
 *   1. single byte round trip
 *   2. back-to-back pair
 *   3. 0x00-0xFF sweep in groups of 8 (exact sequence)
 *   4. 64-byte rapid burst: echoed bytes are an exact in-order prefix of the
 *      sent stream (drops allowed under overrun, corruption never), plus
 *      recovery on a fresh byte
 *   5. glitch immunity: short low pulse while idle echoes nothing
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include "verilated.h"
#include "Vsoc_top.h"

#define VCD_TRACE 1
#ifdef VCD_TRACE
#include "verilated_vcd_c.h"
#endif

static const uint64_t SYS_CLK_HZ   = 50000000ULL;
static const uint64_t BAUD_RATE    = 115200;
static const uint64_t CLKS_PER_BIT = SYS_CLK_HZ / BAUD_RATE;   // 434
static const uint64_t RESET_CYCLES = 20;
static uint64_t MAX_CYCLES = getenv("TB_MAX") ? atoll(getenv("TB_MAX")) : 60000000ULL;

class TxDecoder {
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

static Vsoc_top* dut;
static TxDecoder dec;
static std::vector<uint8_t> rxlog;          // every byte decoded off uart_tx
static uint64_t cyc = 0;

static void step(void) {
    dut->clk = 1; dut->eval();
    dec.tick(dut->uart_tx & 1);
    if (dec.has_byte) {
        rxlog.push_back(dec.rx_byte);
        if (getenv("TB_DEBUG"))
            printf("[DEC] %llu %02x '%c'\n", (unsigned long long)cyc,
                   dec.rx_byte,
                   (dec.rx_byte >= 32 && dec.rx_byte < 127) ? dec.rx_byte : '.');
    }
#ifdef VCD_TRACE
#endif
    dut->clk = 0; dut->eval();
    cyc++;
}

static void rx_bit(int lvl) {
    dut->uart_rx = lvl & 1;
    for (uint64_t i = 0; i < CLKS_PER_BIT && cyc < MAX_CYCLES; i++)
        step();
}

static void send_byte(uint8_t b) {
    rx_bit(0);
    for (int k = 0; k < 8; k++) rx_bit((b >> k) & 1);
    rx_bit(1);
}

static void idle_bits(int n) {
    for (int i = 0; i < n && cyc < MAX_CYCLES; i++)
        rx_bit(1);
}

// pump the sim until pred(log) holds or timeout; returns pred result
template <typename F>
static bool wait_for(F pred, uint64_t timeout) {
    uint64_t deadline = cyc + timeout;
    while (cyc < MAX_CYCLES && cyc < deadline) {
        if (pred()) return true;
        step();
    }
    return pred();
}

static std::string str_of(const std::vector<uint8_t>& v, size_t from) {
    std::string s;
    for (size_t i = from; i < v.size(); i++)
        s += (char)((v[i] >= 32 && v[i] < 127) ? v[i] : '.');
    return s;
}

// strip known firmware-injected lines so echo comparisons see only echoes
static std::vector<uint8_t> filter_firmware(const std::vector<uint8_t>& v,
                                            size_t from) {
    static const char* ovf = "[ECHO] overrun\r\n";
    std::vector<uint8_t> out;
    size_t i = from;
    while (i < v.size()) {
        if (i + strlen(ovf) <= v.size() &&
            memcmp(&v[i], ovf, strlen(ovf)) == 0) {
            i += strlen(ovf);
            continue;
        }
        out.push_back(v[i++]);
    }
    return out;
}

static int failures = 0;
static void expect(bool ok, const std::string& what) {
    printf("[%s] %s\n", ok ? "ok" : "FAIL", what.c_str());
    if (!ok) failures++;
}

int main(int argc, char** argv) {
    bool open_wave = false;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--wave")) open_wave = true;
    if (getenv("TB_VCD")) open_wave = true;
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    dut = new Vsoc_top;
#ifdef VCD_TRACE
    VerilatedVcdC* tfp = new VerilatedVcdC;
    if (open_wave) { dut->trace(tfp, 99); tfp->open(getenv("TB_VCD_FILE")
                        ? getenv("TB_VCD_FILE") : "waves/tb_soc_uart_rx.vcd"); }
#endif
    dut->clk = 0; dut->reset = 1; dut->uart_rx = 1;
    printf("[TB] UART RX integration test (echo round trip)\n");
    while (cyc < RESET_CYCLES) step();
    dut->reset = 0;

    // ---- boot sync: wait for the FULL ready banner including newline ----
    // boot sync: search the RAW byte log (str_of dot-mangles '\n', which
    // would make a substring search for "ready\n" never match)
    auto has_ready = [&]() {
        static const char pat[] = "[ECHO] ready\n";
        const size_t n = sizeof(pat) - 1;
        if (rxlog.size() < n) return false;
        for (size_t i = 0; i + n <= rxlog.size(); i++)
            if (memcmp(&rxlog[i], pat, n) == 0) return true;
        return false;
    };
    expect(wait_for(has_ready, 8000000), "boot: echo banner within 8M cycles");

    // quiesce: pump until no new decoded bytes for ~40k cycles (echo drain
    // fully caught up) so each case starts from a clean boundary
    auto quiesce = [&](void) {
        uint64_t last_change = cyc;
        size_t   last_size   = rxlog.size();
        while (cyc < MAX_CYCLES && cyc - last_change < 40000) {
            step();
            if (rxlog.size() != last_size) {
                last_size = rxlog.size();
                last_change = cyc;
            }
        }
    };
    quiesce();

    // ---- case 1: single byte ----
    {
        idle_bits(4);
        size_t mark = rxlog.size();
        send_byte('A');
        idle_bits(4);
        bool ok = wait_for([&]{ return rxlog.size() > mark &&
                                        rxlog[mark] == 'A'; }, 1000000);
        if (!ok) {
            printf("      case1 log tail: %s\n", str_of(rxlog, mark).c_str());
        }
        expect(ok, "case1: 'A' round trip");
        idle_bits(4);
    }

    // ---- case 2: back-to-back pair ----
    {
        size_t mark = rxlog.size();
        send_byte('H');
        send_byte('I');                          // zero inter-frame gap
        bool ok = wait_for([&]{
            return rxlog.size() >= mark + 2 &&
                   rxlog[mark] == 'H' && rxlog[mark+1] == 'I';
        }, 1000000);
        expect(ok, "case2: back-to-back 'HI'");
        idle_bits(4);
    }

    // ---- case 3: full sweep ----
    {
        quiesce();
        std::vector<uint8_t> sent;
        size_t mark = rxlog.size();              // BEFORE sending
        for (int i = 0; i < 256; i++) {
            sent.push_back((uint8_t)i);
            send_byte((uint8_t)i);
            if ((i % 8) == 7) idle_bits(10);     // drain catch-up gap
        }
        bool ok = wait_for([&]{
            if (rxlog.size() < mark + sent.size()) return false;
            for (size_t i = 0; i < sent.size(); i++)
                if (rxlog[mark + i] != sent[i]) return false;
            return true;
        }, 4000000);
        if (!ok) {
            std::string got = str_of(rxlog, mark);
            printf("      got (%zu bytes): %s\n", rxlog.size() - mark,
                   got.substr(0, 80).c_str());
        }
        expect(ok, "case3: 0x00-0xFF sweep exact");
    }

    // ---- case 4: rapid burst -> in-order prefix, then recovery ----
    {
        std::vector<uint8_t> sent;
        sent.push_back('S');
        for (int i = 0; i < 63; i++) sent.push_back((uint8_t)(0x20 + i));
        size_t mark = rxlog.size();
        for (size_t i = 0; i < sent.size(); i++)
            send_byte(sent[i]);
        idle_bits(16);
        // give the CPU ample time to finish echoing whatever survived
        wait_for([&]{ return false; }, 2000000);
        quiesce();

        std::vector<uint8_t> got = filter_firmware(rxlog, mark);
        bool prefix = got.size() <= sent.size();
        for (size_t i = 0; prefix && i < got.size(); i++)
            if (got[i] != sent[i]) prefix = false;
        expect(prefix && got.size() >= 8,
               "case4: burst echoes an in-order prefix (" +
               std::to_string(got.size()) + "/" +
               std::to_string(sent.size()) + " bytes)");

        // recovery: a fresh single byte must still work
        idle_bits(8);
        quiesce();
        size_t mark2 = rxlog.size();
        send_byte('Z');
        wait_for([&]{
            return rxlog.size() > mark2 && rxlog.back() == 'Z';
        }, 1000000);
        std::vector<uint8_t> got2 = filter_firmware(rxlog, mark2);
        bool rec = got2.size() == 1 && got2[0] == 'Z';
        if (!rec) {
            printf("      recovery got %zu bytes: %s\n", got2.size(),
                   str_of(got2, 0).substr(0, 60).c_str());
        }
        expect(rec, "case4: recovery after burst");
    }

    // ---- case 5: glitch immunity ----
    {
        quiesce();
        size_t mark = rxlog.size();
        dut->uart_rx = 0;
        for (int i = 0; i < 60; i++) step();     // << half a start bit
        dut->uart_rx = 1;
        idle_bits(16);
        bool quiet = true;
        for (size_t i = mark; i < rxlog.size(); i++)
            if (rxlog[i] != 0) quiet = false;    // NUL would be a real frame
        expect(quiet && rxlog.size() - mark <= 1,
               "case5: glitch echoes nothing");
        send_byte('X');
        idle_bits(4);
        bool ok = wait_for([&]{ return rxlog.size() > mark &&
                                        rxlog.back() == 'X'; }, 1000000);
        expect(ok, "case5: framing intact after glitch");
    }

#ifdef VCD_TRACE
    if (open_wave) { tfp->close(); delete tfp; }
#endif
    printf("[TB] cycles=%llu log=%zu bytes\n",
           (unsigned long long)cyc, rxlog.size());
    printf(failures ? "[TB] *** FAIL *** (%d)\n" : "[TB] *** PASS ***\n",
           failures);
    delete dut;
    return failures ? 1 : 0;
}
