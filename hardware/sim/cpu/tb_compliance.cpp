// =============================================================================
// tb_compliance.cpp — DUT harness for riscv-arch-test
// =============================================================================
// Runs one compiled test on the Omni-RISC CPU and dumps the signature region.
//
// Usage:
//   Vcpu_top +hex=<image.hex> +pass=<0x..> +fail=<0x..> +sig=<0x..> +words=<N>
//            +out=<sigfile> [+cycles=<max>]
//
//   image.hex   — objcopy -O verilog of the test ELF (linked at 0x100000).
//                 Loaded into both BRAMs (the 256KB window aliases by
//                 addr[17:2], so 0x100000..0x13ffff wraps to index 0..).
//   pass/fail   — PC addresses of rvmodel_halt_pass/fail spin loops.
//   sig/words   — signature base address and word count to dump.
//   out         — file to write the signature (binary, little-endian words).
//
// The test is done when the PC sits in the pass or fail loop for >=64
// consecutive cycles (64 > longest div stall, so no false trigger).
// =============================================================================

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "Vcpu_top.h"
#include "Vcpu_top___024root.h"
#include "Vcpu_top_cpu_top.h"
#include "Vcpu_top_decode_stage.h"
#include "Vcpu_top_regfile.h"
#include "Vcpu_top_fetch_stage.h"
#include "Vcpu_top_pc_gen.h"
#include "Vcpu_top_instr_bram.h"
#include "Vcpu_top_mem_stage.h"
#include "Vcpu_top_data_bram.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

static Vcpu_top* dut;
static VerilatedVcdC* tfp;
static vluint64_t t = 0;

static void tick() {
    dut->clk = 0; dut->eval(); if (tfp) tfp->dump(t++);
    dut->clk = 1; dut->eval(); if (tfp) tfp->dump(t++);
}

// ---------------------------------------------------------------------------
// Convert an objcopy -O verilog hex (byte-oriented, real addresses) into a
// $readmemh-format word hex indexed by the 256KB-alias: idx=(addr&0x3FFFC)>>2.
// Written as "program.hex" BEFORE construction so instr_bram's $readmemh
// loads it at time zero (same mechanism as tb_cpu_top).
// ---------------------------------------------------------------------------
static void stage_hex(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
    FILE* out = fopen("program.hex", "w");
    if (!out) { fprintf(stderr, "cannot open program.hex\n"); exit(2); }
    char line[512]; uint32_t addr = 0;
    uint32_t buf = 0; int nb = 0;
    uint32_t cur_idx = 0xFFFFFFFF;
    auto flush_word = [&](uint32_t a) {
        uint32_t idx = (a & 0x3FFFC) >> 2;
        if (idx != cur_idx) { fprintf(out, "@%05X\n", idx); cur_idx = idx; }
        fprintf(out, "%08X\n", buf);
    };
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '@') {
            if (nb) { flush_word(addr); nb = 0; }
            addr = (uint32_t)strtoul(line + 1, nullptr, 16);
            continue;
        }
        char* tok = strtok(line, " \t\r\n");
        while (tok) {
            buf = (buf >> 8) | ((uint32_t)strtoul(tok, nullptr, 16) << 24); // little-endian word
            nb++;
            addr++;
            if (nb == 4) { flush_word(addr - 4); nb = 0; }
            tok = strtok(nullptr, " \t\r\n");
        }
    }
    if (nb) flush_word(addr - nb);
    fclose(f); fclose(out);
}

// Parse the objcopy hex again and poke each word into data_bram (aliased).
static void poke_data(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "cannot open %s\n", path); exit(2); }
    char line[512]; uint32_t addr = 0;
    uint32_t buf = 0; int nb = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '@') { if (nb) { uint32_t a = addr - nb; dut->rootp->cpu_top->u_mem->u_dbram->mem[(a & 0x3FFFC)>>2] = buf; nb=0; } addr = (uint32_t)strtoul(line+1, nullptr, 16); continue; }
        char* tok = strtok(line, " \t\r\n");
        while (tok) {
            buf = (buf >> 8) | ((uint32_t)strtoul(tok, nullptr, 16) << 24);
            nb++; addr++;
            if (nb == 4) { uint32_t a = addr - 4; dut->rootp->cpu_top->u_mem->u_dbram->mem[(a & 0x3FFFC)>>2] = buf; nb = 0; }
            tok = strtok(nullptr, " \t\r\n");
        }
    }
    if (nb) { uint32_t a = addr - nb; dut->rootp->cpu_top->u_mem->u_dbram->mem[(a & 0x3FFFC)>>2] = buf; }
    fclose(f);
}

// extract a hex arg like "+name=value"
static const char* get_arg(int argc, char** argv, const char* name) {
    for (int i = 1; i < argc; i++)
        if (strncmp(argv[i], name, strlen(name)) == 0)
            return argv[i] + strlen(name);
    return nullptr;
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    const char* hx = get_arg(argc, argv, "+hex=");
    uint32_t pass_addr = (uint32_t)strtoul(get_arg(argc, argv, "+pass="), nullptr, 0);
    uint32_t fail_addr = (uint32_t)strtoul(get_arg(argc, argv, "+fail="), nullptr, 0);
    uint32_t sig_begin = (uint32_t)strtoul(get_arg(argc, argv, "+sig="), nullptr, 0);
    uint32_t sig_words = (uint32_t)strtoul(get_arg(argc, argv, "+words="), nullptr, 0);
    const char* outfile = get_arg(argc, argv, "+out=");
    uint64_t max_cycles = get_arg(argc, argv, "+cycles=") ? strtoull(get_arg(argc, argv, "+cycles="), nullptr, 0) : 5000000;
    if (get_arg(argc, argv, "+wave=")) max_cycles = 60;  // short run for waveform debug
    if (!hx || !outfile) { fprintf(stderr, "usage: +hex= +pass= +fail= +sig= +words= +out= [+cycles=]\n"); return 2; }

    stage_hex(hx);                 // writes program.hex for instr_bram $readmemh
    dut = new Vcpu_top;            // $readmemh loads program.hex at time zero
    if (get_arg(argc, argv, "+wave=")) {
        Verilated::traceEverOn(true);
        tfp = new VerilatedVcdC;
        dut->trace(tfp, 99);
        tfp->open(get_arg(argc, argv, "+wave="));
    }
    poke_data(hx);                 // data_bram has no readmemh — poke it directly
    if (get_arg(argc, argv, "+dumpmem=1")) {
        fprintf(stderr, "imem[0..5] =");
        for (int i = 0; i < 6; i++) fprintf(stderr, " %08x", dut->rootp->cpu_top->u_fetch->instr_bram1->mem[i]);
        fprintf(stderr, "\n");
    }

    // reset
    dut->reset = 1;
    for (int i = 0; i < 6; i++) tick();
    dut->reset = 0;
    // start at the test entry (linked at TEST_BASE, not the reset vector).
    // The reset's flush masks the first fetched instruction, so let flush
    // deassert for one cycle, then re-arm the PC at the entry.
    const char* entry = get_arg(argc, argv, "+entry=");
    if (entry) {
        tick();  // flush -> 0
        dut->rootp->cpu_top->u_fetch->pc_gen1->pc = (uint32_t)strtoul(entry, nullptr, 0);
    }

    // Completion detection: the test's halt writes 1 (pass) / 3 (fail) to the
    // tohost location (sail-macros RVMODEL_HALT_*), which is deterministic and
    // immune to pipeline fetch-ahead. Fallback: PC staying in the pass/fail
    // loop window for many cycles.
    const char* tohost_s = get_arg(argc, argv, "+tohost=");
    uint32_t tohost = tohost_s ? (uint32_t)strtoul(tohost_s, nullptr, 0) : 0;
    uint32_t tohost_idx = (tohost & 0x3FFFC) >> 2;
    uint64_t pass_hits = 0, fail_hits = 0;
    const char* result = nullptr;
    bool trace = get_arg(argc, argv, "+trace=1") != nullptr;
    for (uint64_t cyc = 0; cyc < max_cycles; cyc++) {
        tick();
        uint32_t pc = dut->rootp->cpu_top->u_fetch->pc_gen1->pc;
        if (trace && cyc < 15) fprintf(stderr, "cyc=%llu pc=0x%x x1=0x%x\n",
            (unsigned long long)cyc, pc,
            dut->rootp->cpu_top->u_decode->regfile1->reg_file[1]);
        if (tohost_s) {
            uint32_t t = dut->rootp->cpu_top->u_mem->u_dbram->mem[tohost_idx];
            if (t == 1) { result = "PASS"; break; }
            if (t == 3) { result = "FAIL"; break; }
        } else {
            if (pc >= pass_addr && pc < pass_addr + 0x40) { pass_hits++; fail_hits = 0; if (pass_hits >= 64) { result = "PASS"; break; } }
            else if (pc >= fail_addr && pc < fail_addr + 0x40) { fail_hits++; pass_hits = 0; if (fail_hits >= 64) { result = "FAIL"; break; } }
            else { pass_hits = fail_hits = 0; }
        }
    }
    if (!result) {
        uint32_t lastpc = dut->rootp->cpu_top->u_fetch->pc_gen1->pc;
        fprintf(stderr, "tb_compliance: no terminal loop within %llu cycles (last pc=0x%x)\n",
                (unsigned long long)max_cycles, lastpc);
        if (tfp) { tfp->close(); }
        return 1;
    }

    // dump the signature region (little-endian words)
    uint32_t base = (sig_begin & 0x3FFFC) >> 2;
    FILE* out = fopen(outfile, "wb");
    if (!out) { fprintf(stderr, "cannot open %s\n", outfile); return 2; }
    for (uint32_t i = 0; i < sig_words; i++) {
        uint32_t w = dut->rootp->cpu_top->u_mem->u_dbram->mem[base + i];
        fwrite(&w, 4, 1, out);
    }
    fclose(out);
    fprintf(stderr, "tb_compliance: %s, %llu cycles\n", result, (unsigned long long)(tfp ? t/2 : 0));
    if (tfp) tfp->close();
    return strcmp(result, "PASS") == 0 ? 0 : 1;
}
