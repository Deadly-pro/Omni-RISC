/**
 * @file tb_soc_shell.cpp
 * @brief Verilator interactive TB: type into the FreeRTOS shell over UART (R3).
 *
 * DUT: soc_top (tb_soc_* mapping). Stages firmware/shell.hex.
 *
 * Unlike every other TB here, this one is interactive:
 *   - uart_rx is fed from stdin at REAL baud timing (434 clk/bit) — the
 *     deserializer must see valid framing, that honesty is the point.
 *   - no fixed cycle cap: runs until the shell's `quit` marker is decoded,
 *     or stdin hits EOF and the TX stream goes quiet; 10G-cycle safety kill.
 *   - typed bytes are echoed to stderr as they are consumed (so a live
 *     session feels like a terminal and piped transcripts stay clean on
 *     stdout); decoded TX goes to stdout in real time.
 *   - after each newline the TB waits for the response to drain (no new TX
 *     byte for QUIESCE_CYCLES) before feeding the next line — a 16-byte RX
 *     FIFO cannot absorb a whole pasted script while the console task sleeps.
 *
 * VCD off by default; --wave or TB_VCD=1 enables it.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>
#include <poll.h>
#include <unistd.h>
#include "verilated.h"
#define VCD_TRACE 1
#include "Vsoc_top.h"

#ifdef VCD_TRACE
#include "verilated_vcd_c.h"
#endif

static const uint64_t SYS_CLK_HZ   = 50000000ULL;
static const uint64_t BAUD_RATE    = 115200;
static const uint64_t CLKS_PER_BIT = SYS_CLK_HZ / BAUD_RATE;   // 434
static const uint64_t RESET_CYCLES = 20;
static const uint64_t SAFETY_CYCLES = 10000000000ULL;          // 10G
static uint64_t MAX_CYCLES = SAFETY_CYCLES;
static const char QUIT_MARK[] = "[SHELL] QUIT";

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

// feeds queued stdin bytes onto uart_rx with valid UART framing
class RxDriver {
public:
    bool     active = false;
    int      phase  = 0;      // 0=start, 1..8=data LSB-first, 9=stop
    uint64_t t      = 0;
    uint8_t  cur    = 0;
    static const uint64_t GAP_BITS = 2;
    uint64_t gap    = 0;
    std::deque<uint8_t> q;
    void push(uint8_t b) { q.push_back(b); }
    bool busy(void) { return active || !q.empty(); }
    int level(void) {
        if (!active) return 1;
        if (phase == 0) return 0;             // start bit
        if (phase >= 9) return 1;             // stop bit
        return (cur >> (phase - 1)) & 1;      // data LSB-first
    }
    int tick(void) {
        if (!active) {
            if (gap > 0) { gap--; return 1; }   // inter-frame idle gap
            if (q.empty()) return 1;
            cur = q.front(); q.pop_front();
            active = true; phase = 0; t = 0;
            return level();
        }
        if (++t < CLKS_PER_BIT) return level();
        t = 0;
        if (phase == 9) {
            active = false;
            gap = GAP_BITS * CLKS_PER_BIT;      // park idle between frames
            return 1;
        }
        phase++;
        return level();
    }
};

static Vsoc_top* dut;
#ifdef VCD_TRACE
static VerilatedVcdC* tfp = nullptr;
#endif
static TxDecoder dec;
static RxDriver  rxd;
static std::vector<uint8_t> rxlog;
static bool  stdin_eof = false;
static bool  drain_line = false;   // just queued '\n': let the response finish
static uint64_t drain_start = 0;   // cycle the newline was queued
static uint64_t cyc = 0;

static bool tb_debug(void) { static int d = -1; if (d < 0) d = getenv("TB_DEBUG") ? 1 : 0; return d == 1; }

static void step(void) {
    dut->uart_rx = rxd.tick() & 1;
    dut->clk = 1; dut->eval();
    dec.tick(dut->uart_tx & 1);
    if (dec.has_byte) {
        rxlog.push_back(dec.rx_byte);
        uint8_t b = dec.rx_byte;
        if (tb_debug() && (b < 32 || b >= 127))
            fprintf(stderr, "[TX-DEC] %llu %02x\n", (unsigned long long)cyc, b);
        putchar(b);
        fflush(stdout);
    }
    dut->clk = 0; dut->eval();
#ifdef VCD_TRACE
    if (tfp) tfp->dump(cyc * 10);          // 20ps clock -> 10ps per half
#endif
    cyc++;
}

// pull stdin ONE LINE at a time (non-blocking); stop queuing at a newline
// so the next line waits for the response to drain — the 16-byte RX FIFO
// cannot absorb a whole pasted script while the console task sleeps.
static void pump_stdin(void) {
    struct pollfd pfd = { .fd = 0, .events = POLLIN, .revents = 0 };
    while (!stdin_eof && !drain_line &&
           poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        uint8_t b;
        ssize_t n = read(0, &b, 1);
        if (n <= 0) { stdin_eof = true; return; }
        rxd.push(b);
        if (tb_debug()) fprintf(stderr, "[RX-FED] %llu %02x '%c'\n", (unsigned long long)cyc, b, (b >= 32 && b < 127) ? b : '.');
        if (b >= 32 && b < 127) fprintf(stderr, "%c", b);
        else if (b == '\n') fprintf(stderr, "\n");
        else fprintf(stderr, "\\x%02x", b);
        fflush(stderr);
        if (b == '\n') { drain_line = true; drain_start = cyc; }
    }
}

static bool log_has(const char* pat) {
    size_t n = strlen(pat);
    if (rxlog.size() < n) return false;
    for (size_t i = rxlog.size() - n + 1; i-- > 0;)
        if (memcmp(&rxlog[i], pat, n) == 0) return true;
    return false;
}

int main(int argc, char** argv) {
    bool open_wave = false;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--wave")) open_wave = true;
    if (getenv("TB_VCD")) open_wave = true;
    if (getenv("TB_MAX")) MAX_CYCLES = atoll(getenv("TB_MAX"));
    setvbuf(stderr, NULL, _IONBF, 0);
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    dut = new Vsoc_top;
#ifdef VCD_TRACE
    tfp = new VerilatedVcdC;
    if (open_wave) { dut->trace(tfp, 99); tfp->open(getenv("TB_VCD_FILE")
                        ? getenv("TB_VCD_FILE") : "waves/tb_soc_shell.vcd"); }
#endif
    dut->clk = 0; dut->reset = 1; dut->uart_rx = 1;
    printf("[SHELL-TB] interactive session (EOF or `quit` ends it)\n");
    while (cyc < RESET_CYCLES) step();
    dut->reset = 0;

    // boot: wait for the shell banner before accepting input
    while (cyc < MAX_CYCLES && !log_has("[SHELL] ready")) step();
    if (!log_has("[SHELL] ready")) {
        fprintf(stderr, "\n[SHELL-TB] FAIL: banner never appeared\n");
        return 1;
    }

    bool quit_seen = false;
    uint64_t last_tx_change = cyc;
    size_t   last_log_size  = rxlog.size();

    while (cyc < MAX_CYCLES) {
        step();
        if (rxlog.size() != last_log_size) {
            last_log_size = rxlog.size();
            last_tx_change = cyc;
        }
        if (log_has(QUIT_MARK)) { quit_seen = true; break; }

        if (drain_line) {
            // quiesce: no new TX byte for ~6ms of sim time -> next line
            // release >=4ms after the newline was queued AND TX quiet
            // >=2ms: the console task may sleep up to 2ms before it even
            // starts responding, so global TX silence alone would release
            // while the answer is still pending
            if (!rxd.busy() && cyc - drain_start > 200000 &&
                cyc - last_tx_change > 100000)
                drain_line = false;
            continue;
        }
        if (stdin_eof) {
            // all input consumed: run until quiet, then end cleanly
            if (!rxd.busy() && cyc - last_tx_change > 3000000) break;
        }
        pump_stdin();
    }
#ifdef VCD_TRACE
    if (open_wave) tfp->close();
#endif
    printf("\n[SHELL-TB] done at %llu cycles (%s)\n", (unsigned long long)cyc,
           quit_seen ? "quit command" : (stdin_eof ? "stdin EOF" : "safety kill"));
    return 0;
}
