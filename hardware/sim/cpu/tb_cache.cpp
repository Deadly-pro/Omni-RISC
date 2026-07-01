// =============================================================================
// tb_cache.cpp — Verilator testbench for Omni-RISC L1 D-Cache
// =============================================================================
//
// DUT: l1_dcache (hardware/rtl/cpu/cache/l1_dcache.v)
//
// Port map:
//   input         clk, reset
//   CPU interface:
//     input  [31:0] addr
//     input  [31:0] wdata
//     input  [3:0]  byte_en     (byte enables: SB=0001/0010/0100/1000,
//                                              SH=0011/1100, SW=1111)
//     input         read_en, write_en
//     output [31:0] rdata
//     output        hit, miss, ready
//   Memory interface:
//     output [31:0] mem_addr
//     input  [31:0] mem_rdata
//     output        mem_read_req
//     input         mem_read_ack
//
// Cache parameters (from arch spec):
//   4 KB total, 2-way set-associative, write-through, no allocate on write
//   32-byte cache line (8 words)
//   64 sets  (4096 / 2 ways / 32 bytes)
//
// Tests:
//   1. Cold miss → fill → hit
//   2. Write-through: write reaches memory immediately
//   3. Eviction on 2-way conflict (three addresses to same set)
//   4. Byte / halfword / word access via byte_en
//   5. MMIO bypass (addr >= 0x10000000 always misses, not cached)
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <map>

#include "Vl1_dcache.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

// ---------------------------------------------------------------------------
// Simulated main memory — backing store for the memory interface
// ---------------------------------------------------------------------------
static std::map<uint32_t, uint32_t> main_memory;

static uint32_t mem_read(uint32_t addr) {
    addr &= ~3u;  // word-align
    auto it = main_memory.find(addr);
    return (it != main_memory.end()) ? it->second : 0;
}

static void mem_write(uint32_t addr, uint32_t data, uint32_t byte_en) {
    addr &= ~3u;
    uint32_t old = mem_read(addr);
    uint32_t mask = 0;
    if (byte_en & 1) mask |= 0x000000FF;
    if (byte_en & 2) mask |= 0x0000FF00;
    if (byte_en & 4) mask |= 0x00FF0000;
    if (byte_en & 8) mask |= 0xFF000000;
    main_memory[addr] = (old & ~mask) | (data & mask);
}

// ---------------------------------------------------------------------------
// Clock and memory-interface simulation
// ---------------------------------------------------------------------------
static vluint64_t sim_time = 0;

// Tick one clock cycle and drive the memory interface:
//   - When mem_read_req goes high, we respond with mem_rdata + mem_read_ack
//     after a configurable latency (here: 2 cycles to simulate real SRAM).
static int  mem_latency_counter = 0;
static bool mem_pending = false;
static uint32_t mem_pending_addr = 0;

static void tick(Vl1_dcache* dut, VerilatedVcdC* tfp) {
    // Falling edge
    dut->clk = 0;
    dut->eval();
    tfp->dump(sim_time++);

    // Drive memory interface
    if (dut->mem_read_req && !mem_pending) {
        // New memory request
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

// Tick until 'ready' is asserted (with timeout)
static bool wait_ready(Vl1_dcache* dut, VerilatedVcdC* tfp, int max_cycles = 50) {
    for (int i = 0; i < max_cycles; i++) {
        tick(dut, tfp);
        if (dut->ready) return true;
    }
    return false;  // timeout
}

// Apply reset
static void do_reset(Vl1_dcache* dut, VerilatedVcdC* tfp) {
    dut->reset    = 1;
    dut->read_en  = 0;
    dut->write_en = 0;
    dut->addr     = 0;
    dut->wdata    = 0;
    dut->byte_en  = 0;
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

// Perform a cache read and return the result
static uint32_t cache_read(Vl1_dcache* dut, VerilatedVcdC* tfp,
                            uint32_t addr, uint8_t byte_en = 0xF) {
    dut->addr     = addr;
    dut->byte_en  = byte_en;
    dut->read_en  = 1;
    dut->write_en = 0;
    tick(dut, tfp);
    dut->read_en = 0;
    wait_ready(dut, tfp);
    return dut->rdata;
}

// Perform a cache write
static void cache_write(Vl1_dcache* dut, VerilatedVcdC* tfp,
                         uint32_t addr, uint32_t data, uint8_t byte_en = 0xF) {
    dut->addr     = addr;
    dut->wdata    = data;
    dut->byte_en  = byte_en;
    dut->write_en = 1;
    dut->read_en  = 0;
    tick(dut, tfp);
    dut->write_en = 0;
    wait_ready(dut, tfp);
}

// =============================================================================
// Main
// =============================================================================
int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    Vl1_dcache* dut = new Vl1_dcache;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
#ifdef VCD_FILE
    tfp->open(VCD_FILE);
#else
    tfp->open("waves/tb_cache.vcd");
#endif

    printf("============================================================\n");
    printf("  Omni-RISC L1 D-Cache Testbench\n");
    printf("============================================================\n\n");

    do_reset(dut, tfp);

    // Pre-populate backing memory so cold reads return known data
    main_memory[0x00001000] = 0xDEADBEEF;
    main_memory[0x00001004] = 0xCAFEBABE;
    main_memory[0x00001008] = 0x12345678;

    // =========================================================================
    // TEST 1: Cold miss → fill → hit
    // =========================================================================
    printf("--- Test 1: Cold miss → fill → hit ---\n");

    // First read to 0x1000: expect cache miss (cold start), data fetched from mem
    dut->addr    = 0x00001000;
    dut->read_en = 1;
    dut->write_en = 0;
    dut->byte_en = 0xF;
    tick(dut, tfp);

    // On first cycle of a cold miss, 'miss' should be asserted
    // (depending on RTL, miss may be combinational or registered)
    // We just wait for ready and check the data
    dut->read_en = 0;
    wait_ready(dut, tfp);
    check("Cold read 0x1000 = 0xDEADBEEF", 0xDEADBEEF, dut->rdata);

    // Second read to same address: should be a hit (fast, 1-cycle)
    dut->addr    = 0x00001000;
    dut->read_en = 1;
    tick(dut, tfp);

    // On a hit, ready should be immediate (same cycle or next cycle)
    check_bool("Second read 0x1000 is a hit", true, dut->hit);
    dut->read_en = 0;
    tick(dut, tfp);

    // =========================================================================
    // TEST 2: Write-through — write goes to both cache and memory
    // =========================================================================
    printf("\n--- Test 2: Write-through behaviour ---\n");

    // Write 0xAAAAAAAA to address 0x1000 (already cached)
    cache_write(dut, tfp, 0x00001000, 0xAAAAAAAA);

    // Read back from cache — should get new value
    uint32_t cached_val = cache_read(dut, tfp, 0x00001000);
    check("Cache read-back after write = 0xAAAAAAAA", 0xAAAAAAAA, cached_val);

    // Check backing memory was also updated (write-through)
    check("Memory updated (write-through) = 0xAAAAAAAA",
          0xAAAAAAAA, main_memory[0x00001000]);

    // =========================================================================
    // TEST 3: Eviction on 2-way conflict
    // =========================================================================
    printf("\n--- Test 3: Eviction on 2-way set conflict ---\n");

    // For a 2-way, 64-set cache with 32-byte lines:
    //   set index = (addr >> 5) & 0x3F
    //   Address 0x00002000 → set = (0x2000 >> 5) & 0x3F = 0x00
    //   Address 0x00002800 → set = (0x2800 >> 5) & 0x3F = 0x00  (same set!)
    //   Address 0x00003000 → set = (0x3000 >> 5) & 0x3F = 0x00  (same set!)
    //
    // First two addresses fill both ways; third causes eviction.
    uint32_t addr_a = 0x00002000;
    uint32_t addr_b = 0x00002800;
    uint32_t addr_c = 0x00003000;

    main_memory[addr_a] = 0x11111111;
    main_memory[addr_b] = 0x22222222;
    main_memory[addr_c] = 0x33333333;

    // Fill way 0
    uint32_t val = cache_read(dut, tfp, addr_a);
    check("Read addr_a = 0x11111111", 0x11111111, val);

    // Fill way 1
    val = cache_read(dut, tfp, addr_b);
    check("Read addr_b = 0x22222222", 0x22222222, val);

    // addr_c maps to same set — should evict one of the ways
    val = cache_read(dut, tfp, addr_c);
    check("Read addr_c (eviction) = 0x33333333", 0x33333333, val);

    // addr_a or addr_b should now miss (one was evicted)
    // Read addr_a — if evicted, it's a miss and will be re-fetched from memory
    val = cache_read(dut, tfp, addr_a);
    check("Re-read addr_a after eviction = 0x11111111", 0x11111111, val);

    // =========================================================================
    // TEST 4: Byte / halfword / word access via byte_en
    // =========================================================================
    printf("\n--- Test 4: Byte/halfword/word access ---\n");

    uint32_t test_addr = 0x00004000;
    main_memory[test_addr] = 0x00000000;

    // Write a full word first
    cache_write(dut, tfp, test_addr, 0x00000000, 0xF);

    // SB: write byte 0xAB to byte 0 (byte_en = 0001)
    cache_write(dut, tfp, test_addr, 0x000000AB, 0x1);
    val = cache_read(dut, tfp, test_addr);
    check("SB byte0: wrote 0xAB → word = 0x000000AB", 0x000000AB, val);

    // SB: write byte 0xCD to byte 1 (byte_en = 0010)
    cache_write(dut, tfp, test_addr, 0x0000CD00, 0x2);
    val = cache_read(dut, tfp, test_addr);
    check("SB byte1: wrote 0xCD → word = 0x0000CDAB", 0x0000CDAB, val);

    // SH: write halfword 0xEF12 to upper half (byte_en = 1100)
    cache_write(dut, tfp, test_addr, 0xEF120000, 0xC);
    val = cache_read(dut, tfp, test_addr);
    check("SH upper: wrote 0xEF12 → word = 0xEF12CDAB", 0xEF12CDAB, val);

    // SW: overwrite entire word (byte_en = 1111)
    cache_write(dut, tfp, test_addr, 0xFEDCBA98, 0xF);
    val = cache_read(dut, tfp, test_addr);
    check("SW: wrote 0xFEDCBA98", 0xFEDCBA98, val);

    // =========================================================================
    // TEST 5: MMIO bypass (addr >= 0x10000000)
    // =========================================================================
    printf("\n--- Test 5: MMIO bypass ---\n");

    uint32_t mmio_addr = 0x10000004;
    main_memory[mmio_addr] = 0x42424242;

    // Read MMIO address — should always go to memory (no caching)
    val = cache_read(dut, tfp, mmio_addr);
    check("MMIO read 0x10000004 = 0x42424242", 0x42424242, val);

    // Write to MMIO, then change backing memory, then read again
    // If not cached, we should see the backing memory's latest value
    cache_write(dut, tfp, mmio_addr, 0x99999999);
    main_memory[mmio_addr] = 0xBBBBBBBB;  // External agent changes it
    val = cache_read(dut, tfp, mmio_addr);
    check("MMIO re-read sees latest memory = 0xBBBBBBBB", 0xBBBBBBBB, val);

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
