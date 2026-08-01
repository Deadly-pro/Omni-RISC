// =============================================================================
// tb_icache.cpp — Verilator testbench for Omni-RISC L1 I-Cache
// =============================================================================
//
// DUT: l1_icache (hardware/rtl/cpu/cache/l1_icache.v)
//
// Port map:
//   input         clk, reset
//   CPU interface:
//     input  [31:0] pc         (fetch address)
//     output [31:0] rdata      (registered read — instr_bram-compatible on hits)
//     output        miss       (1 = refill in flight → freeze pipeline)
//   Memory interface:
//     output [31:0] mem_addr
//     input  [31:0] mem_rdata
//     output        mem_read_req
//     input         mem_read_ack
//
// Cache parameters (from arch spec):
//   4 KB total, direct-mapped, read-only
//   32-byte cache line (8 words), 128 lines
//     index = pc[11:5], tag = pc[31:12], word = pc[4:2]
//
// Tests:
//   1. Cold miss → refill → hit (same address twice; second is a hit)
//   2. Line fill: all 8 words of a line hit after the first fills it
//   3. Direct-map conflict: two tags sharing an index evict each other
//   4. 'miss' is asserted while a cold miss is being serviced
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>

#include "Vl1_icache.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// Simulated instruction memory — backing store for the memory interface
// ---------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> main_memory;

static uint32_t mem_read(uint32_t addr) {
    addr &= ~3u;  // word-align
    auto it = main_memory.find(addr);
    return (it != main_memory.end()) ? it->second : 0;
}

// ---------------------------------------------------------------------------
// Clock and memory-interface simulation (2-cycle latency, like tb_cache)
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

static int  mem_latency_counter = 0;
static bool mem_pending = false;
static uint32_t mem_pending_addr = 0;

static void tick(Vl1_icache* dut, VerilatedVcdC* tfp) {
    // Falling edge
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    // Drive memory interface
    if (dut->mem_read_req && !mem_pending) {
        mem_pending = true;
        mem_pending_addr = dut->mem_addr;
        mem_latency_counter = 2;  // 2-cycle latency
        dut->mem_read_ack = 0;
    }

    if (mem_pending) {
        mem_latency_counter--;
        if (mem_latency_counter <= 0) {
            dut->mem_rdata    = mem_read(mem_pending_addr);
            dut->mem_read_ack = 1;
            mem_pending = false;
        }
    } else {
        dut->mem_read_ack = 0;
    }

    // Rising edge
    dut->clk = 1;
    dut->eval();
    tfp->dump(sim_time++);
}

// Apply reset
static void do_reset(Vl1_icache* dut, VerilatedVcdC* tfp) {
    dut->reset = 1;
    dut->pc    = 0;
    dut->fetch_en = 1;
    dut->mem_rdata    = 0;
    dut->mem_read_ack = 0;
    mem_pending = false;
    for (int i = 0; i < 4; i++) tick(dut, tfp);
    dut->reset = 0;
    tick(dut, tfp);
}

// ---------------------------------------------------------------------------
// Test helpers
// ---------------------------------------------------------------------------
static int pass_count = 0;
static int fail_count = 0;

static void check(const char* label, uint32_t expected, uint32_t got) {
    if (expected == got) {
        pass_count++;
        printf("  [PASS] %s\n", label);
    } else {
        fail_count++;
        printf("  [FAIL] %s — expected 0x%08X, got 0x%08X\n",
               label, expected, got);
    }
}

static void check_bool(const char* label, bool expected, bool got) {
    if (expected == got) {
        pass_count++;
        printf("  [PASS] %s\n", label);
    } else {
        fail_count++;
        printf("  [FAIL] %s — expected %d, got %d\n",
               label, (int)expected, (int)got);
    }
}

// Present a fetch address and run until the cache settles:
//   - a hit registers rdata in ~1-2 cycles, 'miss' never asserts
//   - a miss asserts 'miss' until the refill lands, then one IDLE hit-read
//     cycle presents the refilled instruction in rdata
static uint32_t cache_fetch(Vl1_icache* dut, VerilatedVcdC* tfp,
                            uint32_t pc, bool* saw_miss) {
    dut->pc = pc;
    *saw_miss = false;
    bool settled = false;
    for (int i = 0; i < 60; i++) {
        tick(dut, tfp);
        if (dut->miss) *saw_miss = true;
        if (!dut->miss && i >= 1) { settled = true; break; }
    }
    // one extra cycle so a refill's IDLE hit-read lands in rdata
    if (settled) tick(dut, tfp);
    return dut->rdata;
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vl1_icache* dut = new Vl1_icache;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_icache.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC L1 I-Cache Testbench\n");
    printf("============================================================\n\n");

    do_reset(dut, tfp);

    // Pre-populate backing memory
    for (uint32_t i = 0; i < 8; i++)
        main_memory[0x00001000 + i * 4] = 0x10000000 + i;  // instr words
    main_memory[0x00001800] = 0xCAFEBABE;                  // eviction victim
    main_memory[0x00002800] = 0xDEADBEEF;                  // same index, other tag

    // =========================================================================
    // TEST 1: Cold miss → refill → hit
    // =========================================================================
    printf("--- Test 1: Cold miss → refill → hit ---\n");

    bool saw_miss = false;
    uint32_t val = cache_fetch(dut, tfp, 0x00001000, &saw_miss);
    check("Cold fetch 0x1000 = instr word 0", 0x10000000, val);
    check_bool("Cold fetch asserted 'miss'", true, saw_miss);

    // Second fetch to the same address: should be a hit (no miss, same value)
    saw_miss = false;
    val = cache_fetch(dut, tfp, 0x00001000, &saw_miss);
    check("Second fetch 0x1000 = instr word 0", 0x10000000, val);
    check_bool("Second fetch was a hit (no miss)", false, saw_miss);

    // =========================================================================
    // TEST 2: Line fill — all 8 words of the line hit after the first fills it
    // =========================================================================
    printf("\n--- Test 2: Line fill (8 words) ---\n");

    for (uint32_t w = 0; w < 8; w++) {
        uint32_t addr = 0x00001000 + w * 4;
        saw_miss = false;
        val = cache_fetch(dut, tfp, addr, &saw_miss);
        char label[64];
        snprintf(label, sizeof label, "Line word %u = 0x%08X (hit)",
                 w, 0x10000000u + w);
        check(label, 0x10000000u + w, val);
        check_bool("Line word hit (no miss)", false, saw_miss);
    }

    // =========================================================================
    // TEST 3: Direct-map conflict — two tags sharing an index evict each other
    // =========================================================================
    printf("\n--- Test 3: Direct-map conflict eviction ---\n");

    // For a DIRECT-MAP eviction we need SAME index, DIFFERENT tag:
    //   0x00001000 → index = (0x1000 >> 5) & 0x7F = 0x00, tag = 0x00000
    //   0x10001000 → index = (0x10001000 >> 5) & 0x7F = 0x00, tag = 0x10000
    main_memory[0x10001000] = 0x55555555;   // same index 0x00, tag 0x10000

    saw_miss = false;
    val = cache_fetch(dut, tfp, 0x10001000, &saw_miss);
    check("Fetch 0x10001000 (same index, new tag) = 0x55555555", 0x55555555, val);
    check_bool("Conflict fetch asserted 'miss'", true, saw_miss);

    // The line at index 0x00 now holds tag 0x10000; refetching 0x00001000 must
    // miss again (its line was evicted) and re-fill from memory.
    saw_miss = false;
    val = cache_fetch(dut, tfp, 0x00001000, &saw_miss);
    check("Refetch evicted 0x1000 = instr word 0", 0x10000000, val);
    check_bool("Evicted line refetch asserted 'miss'", true, saw_miss);

    // =========================================================================
    // TEST 4: 'miss' stays asserted through the whole refill
    // =========================================================================
    printf("\n--- Test 4: miss asserted during a cold refill ---\n");

    main_memory[0x00002000] = 0xABCDEF01;
    dut->pc = 0x00002000;
    int miss_cycles = 0;
    int first_miss = -1;
    for (int i = 0; i < 60; i++) {
        tick(dut, tfp);
        if (dut->miss) {
            miss_cycles++;
            if (first_miss < 0) first_miss = i;
        }
        if (!dut->miss && i >= 1) { tick(dut, tfp); break; }
    }
    check("Refill of 0x2000 landed the instruction", 0xABCDEF01, dut->rdata);
    check_bool("miss asserted during refill", true, miss_cycles > 0);
    check_bool("miss asserted for multiple cycles (refill is multi-cycle)",
               true, miss_cycles >= 2);

    // =========================================================================
    // Summary
    // =========================================================================
    printf("\n============================================================\n");
    printf("  Results: %d PASSED, %d FAILED\n", pass_count, fail_count);
    printf("  Simulation cycles: %lu\n", (unsigned long)(sim_time / 2));
    printf("============================================================\n");

    tfp->close();
    delete tfp;
    delete dut;

    return (fail_count > 0) ? 1 : 0;
}
