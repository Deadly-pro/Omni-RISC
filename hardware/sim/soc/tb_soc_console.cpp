/**
 * @file tb_soc_console.cpp
 * @brief Generic console harness for ANY firmware image (F0 of the firmware track).
 *
 * DUT: soc_top (tb_soc_* mapping in hardware/sim/run_sim.sh).
 *
 * Unlike tb_soc_shell, this TB knows nothing about the application:
 *   - It runs whatever image is staged as program.hex in its own obj_dir.
 *   - It does NOT wait for a banner, so a firmware that never prints anything
 *     does not hang the harness at the safety cap in silence: it prints a
 *     heartbeat on stderr and ends at TB_MAX with a non-zero exit code.
 *   - UART TX is decoded at real 115200 framing (434 clk/bit) and written raw
 *     to stdout, so a gate can byte-exact diff the firmware's output.
 *   - stdin is fed onto uart_rx at real framing. Pacing:
 *       TB_PACE=burst (default) - back-to-back frames (a genuine FIFO burst)
 *       TB_PACE=line            - one line at a time, waiting for TX to quiesce
 *                                 (for firmware with a sleepy console task)
 *
 * End conditions (whichever comes first):
 *   1. the end marker appears in the TX stream (TB_END_MARK, default
 *      "[TEST] DONE") -> exit 0
 *   2. stdin hits EOF and TX has been quiet for TB_QUIET cycles -> exit 0
 *   3. TB_MAX cycles -> exit 1 (a hung or mis-built image is a gate failure)
 *
 * Env knobs: TB_MAX TB_QUIET TB_PACE TB_END_MARK TB_ECHO TB_DEBUG TB_VCD
 *            TB_HEARTBEAT TB_VCD_FILE.  --wave also enables the VCD.
 */
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <deque>
#include <vector>
#include <poll.h>
#include <unistd.h>
#include "verilated.h"
#define VCD_TRACE 1
#include "Vsoc_top.h"
#ifdef VCD_TRACE
#include "verilated_vcd_c.h"
#endif

static const uint64_t SYS_CLK_HZ    = 50000000ULL;
static const uint64_t BAUD_RATE     = 115200;
static const uint64_t CLKS_PER_BIT  = SYS_CLK_HZ / BAUD_RATE;   // 434
static const uint64_t RESET_CYCLES  = 20;

static uint64_t MAX_CYCLES  = 50000000ULL;    // TB_MAX: 1 s of sim time
static uint64_t QUIET_CYCLES = 3000000ULL;    // TB_QUIET: 60 ms of TX silence
static uint64_t HEARTBEAT   = 10000000ULL;    // TB_HEARTBEAT, 0 disables
static const char* END_MARK = "[TEST] DONE";  // TB_END_MARK
static bool     PACE_LINE   = false;          // TB_PACE=line
static bool     ECHO_INPUT  = false;          // TB_ECHO=1

// ---------------------------------------------------------------------------
// UART framing (same decoder as tb_soc_shell: mid-start-bit resample, 8N1)
// ---------------------------------------------------------------------------
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
        if (phase == 0) return 0;
        if (phase >= 9) return 1;
        return (cur >> (phase - 1)) & 1;
    }
    int tick(void) {
        if (!active) {
            if (gap > 0) { gap--; return 1; }
            if (q.empty()) return 1;
            cur = q.front(); q.pop_front();
            active = true; phase = 0; t = 0;
            return level();
        }
        if (++t < CLKS_PER_BIT) return level();
        t = 0;
        if (phase == 9) {
            active = false;
            gap = GAP_BITS * CLKS_PER_BIT;
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
static std::vector<uint8_t> txlog;
static bool      stdin_eof   = false;
static bool      drain_line  = false;   // PACE_LINE: newline queued, waiting out the reply
static uint64_t  drain_start = 0;
static uint64_t  cyc         = 0;
static bool      live_tty    = false;

static bool tb_debug(void) { static int d = -1; if (d < 0) d = getenv("TB_DEBUG") ? 1 : 0; return d == 1; }

static void step(void) {
    dut->uart_rx = rxd.tick() & 1;
    dut->clk = 1; dut->eval();
    dec.tick(dut->uart_tx & 1);
    if (dec.has_byte) {
        txlog.push_back(dec.rx_byte);
        uint8_t b = dec.rx_byte;
        if (tb_debug() && (b < 32 || b >= 127))
            fprintf(stderr, "[TX-DEC] %llu %02x\n", (unsigned long long)cyc, b);
        putchar(b);
        fflush(stdout);
    }
    dut->clk = 0; dut->eval();
#ifdef VCD_TRACE
    if (tfp) tfp->dump(cyc * 10);
#endif
    cyc++;
}

static void pump_stdin(void) {
    struct pollfd pfd = { .fd = 0, .events = POLLIN, .revents = 0 };
    while (!stdin_eof && !drain_line &&
           poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
        uint8_t b;
        ssize_t n = read(0, &b, 1);
        if (n <= 0) { stdin_eof = true; return; }
        rxd.push(b);
        if (tb_debug())
            fprintf(stderr, "[RX-FED] %llu %02x\n", (unsigned long long)cyc, b);
        if (ECHO_INPUT && !live_tty) {
            if (b >= 32 && b < 127) fprintf(stderr, "%c", b);
            else if (b == '\n')     fprintf(stderr, "\n");
            else                    fprintf(stderr, "\\x%02x", b);
            fflush(stderr);
        }
        if (PACE_LINE && b == '\n') { drain_line = true; drain_start = cyc; }
    }
}

static bool log_has(const char* pat) {
    size_t n = strlen(pat);
    if (txlog.size() < n) return false;
    for (size_t i = txlog.size() - n + 1; i-- > 0;)
        if (memcmp(&txlog[i], pat, n) == 0) return true;
    return false;
}

int main(int argc, char** argv) {
    bool open_wave = false;
    for (int i = 1; i < argc; i++)
        if (!strcmp(argv[i], "--wave")) open_wave = true;

    if (const char* e = getenv("TB_MAX"))       MAX_CYCLES   = strtoull(e, nullptr, 0);
    if (const char* e = getenv("TB_QUIET"))      QUIET_CYCLES = strtoull(e, nullptr, 0);
    if (const char* e = getenv("TB_HEARTBEAT"))  HEARTBEAT    = strtoull(e, nullptr, 0);
    if (const char* e = getenv("TB_END_MARK"))   END_MARK     = e;
    if (const char* e = getenv("TB_PACE"))       PACE_LINE    = !strcmp(e, "line");
    if (const char* e = getenv("TB_ECHO"))       ECHO_INPUT   = atoi(e) != 0;
    if (getenv("TB_VCD")) open_wave = true;

    setvbuf(stderr, nullptr, _IONBF, 0);
    live_tty = isatty(STDIN_FILENO);
    Verilated::commandArgs(argc, argv);
    Verilated::traceEverOn(true);
    dut = new Vsoc_top;
#ifdef VCD_TRACE
    tfp = new VerilatedVcdC;
    if (open_wave) {
        dut->trace(tfp, 99);
        tfp->open(getenv("TB_VCD_FILE") ? getenv("TB_VCD_FILE")
                                        : "waves/tb_soc_console.vcd");
    }
#endif
    dut->clk = 0; dut->reset = 1; dut->uart_rx = 1;
    fprintf(stderr, "[TB] generic console: end marker \"%s\", pace=%s, max=%llu cycles\n",
            END_MARK, PACE_LINE ? "line" : "burst", (unsigned long long)MAX_CYCLES);

    while (cyc < RESET_CYCLES) step();
    dut->reset = 0;

    bool     saw_mark   = false;
    bool     eof_quiet  = false;
    bool     hit_cap    = false;
    uint64_t last_tx    = cyc;
    size_t   last_len   = txlog.size();
    uint64_t next_beat  = HEARTBEAT ? HEARTBEAT : ~0ULL;

    while (cyc < MAX_CYCLES) {
        step();

        if (txlog.size() != last_len) { last_len = txlog.size(); last_tx = cyc; }

        if (log_has(END_MARK)) { saw_mark = true; break; }

        if (HEARTBEAT && cyc >= next_beat) {
            next_beat = cyc + HEARTBEAT;
            fprintf(stderr, "[TB] heartbeat %llu cycles, %zu TX bytes\n",
                    (unsigned long long)cyc, txlog.size());
        }

        if (PACE_LINE && drain_line) {
            // wait for the reply to drain before feeding the next line
            if (!rxd.busy() && cyc - drain_start > 200000 && cyc - last_tx > 100000)
                drain_line = false;
            continue;
        }
        if (stdin_eof) {
            // no more input: end once the firmware has finished talking
            if (cyc - last_tx > QUIET_CYCLES) { eof_quiet = true; break; }
        }
        pump_stdin();
    }
    hit_cap = (cyc >= MAX_CYCLES) && !saw_mark && !eof_quiet;

#ifdef VCD_TRACE
    if (open_wave) tfp->close();
#endif
    fprintf(stderr, "[TB] done at %llu cycles (%s), %zu bytes of UART TX\n",
            (unsigned long long)cyc,
            saw_mark ? "end marker" : (eof_quiet ? "stdin EOF + quiet"
                                                 : "safety cap hit"),
            txlog.size());
    if (hit_cap) {
        fprintf(stderr, "[TB] FAIL: no \"%s\" marker and TX never went quiet - "
                        "image hung, mis-built, or the marker is missing\n", END_MARK);
        return 1;
    }
    return 0;
}
