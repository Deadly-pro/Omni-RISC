// ref_sim.cpp — minimal RV32IM(+Zicsr) interpreter, used as the reference
// model for riscv-arch-test signature comparison.
//
// This is an independent implementation (C++, written from the RISC-V ISA
// spec) of the instructions the ACT tests exercise. It loads a test ELF in
// Verilog-hex form, executes it, and when the PC reaches the pass loop it
// dumps [sig_begin, sig_begin + sig_words*4) to a file.
//
// Usage:
//   ref_sim <test.hex> <pass_addr> <fail_addr> <sig_begin> <sig_words> <out>
//
// Addresses are hex (0x...). The pass/fail loops are where the test's
// rvmodel_halt_pass / rvmodel_halt_fail spin (write_tohost loop).
//
// Coverage: RV32 base-I, M, Zicsr, ecall/ebreak/mret, FENCE/FENCE.I (nop).

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static uint32_t regs[32];
static uint32_t pc;
static uint64_t memsize = 0x200000;             // 2MB flat guest memory (tests link at 1MB)
static std::vector<uint8_t> mem;

static uint64_t cycles = 0;
static const uint64_t MAX_CYCLES = 100000000;

// ---- CSRs (subset the Zicsr tests touch) ----
static uint32_t mstatus, mie, mtvec, mscratch, mepc, mcause, mtval, mip;
static uint32_t mcycle, minstret;

static uint32_t rd_mem32(uint32_t a) { return (uint32_t)mem[a] | ((uint32_t)mem[a+1]<<8) | ((uint32_t)mem[a+2]<<16) | ((uint32_t)mem[a+3]<<24); }
static uint32_t rd_mem16(uint32_t a) { return (uint32_t)mem[a] | ((uint32_t)mem[a+1]<<8); }
static void     wr_mem32(uint32_t a, uint32_t v) { mem[a]=v; mem[a+1]=v>>8; mem[a+2]=v>>16; mem[a+3]=v>>24; }

static int32_t  i_imm(uint32_t insn) { return (int32_t)insn >> 20; }              // I-type: insn[31:20]
static int32_t  s_imm(uint32_t insn) { return (((int32_t)insn >> 25) << 5) | ((insn >> 7) & 0x1f); } // S-type
static int32_t  b_imm(uint32_t insn) { return (((int32_t)insn >> 31) << 12) | (((insn >> 7) & 1) << 11) | (((insn >> 25) & 0x3f) << 5) | (((insn >> 8) & 0xf) << 1); }
static int32_t  j_imm(uint32_t insn) { return (((int32_t)insn >> 31) << 20) | (((insn >> 12) & 0xff) << 12) | (((insn >> 20) & 1) << 11) | (((insn >> 21) & 0x3ff) << 1); }

// load a verilog-format hex from objcopy -O verilog:
//   @ADDR  then lines of space-separated bytes (memory order)
static bool load_hex(const char* path) {
    FILE* f = fopen(path, "r");
    if (!f) { fprintf(stderr, "ref_sim: cannot open %s\n", path); return false; }
    char line[512]; uint32_t addr = 0;
    while (fgets(line, sizeof line, f)) {
        if (line[0] == '@') { addr = (uint32_t)strtoul(line + 1, nullptr, 16); continue; }
        char* tok = strtok(line, " \t\r\n");
        while (tok) {
            if (addr < memsize) mem[addr++] = (uint8_t)strtoul(tok, nullptr, 16);
            tok = strtok(nullptr, " \t\r\n");
        }
    }
    fclose(f);
    return true;
}

static uint32_t csr_read(uint32_t a) {
    switch (a) {
        case 0x300: return mstatus;
        case 0x304: return mie;
        case 0x305: return mtvec;
        case 0x340: return mscratch;
        case 0x341: return mepc;
        case 0x342: return mcause;
        case 0x343: return mtval;
        case 0x344: return mip;
        case 0xB00: return mcycle;
        case 0xB02: return minstret;
        case 0xF11: case 0xF12: case 0xF13: case 0xF14: return 0;
        default: return 0;
    }
}
static void csr_write(uint32_t a, uint32_t v) {
    switch (a) {
        case 0x300: mstatus = v; break;
        case 0x304: mie = v; break;
        case 0x305: mtvec = v; break;
        case 0x340: mscratch = v; break;
        case 0x341: mepc = v; break;
        case 0x342: mcause = v; break;
        case 0x343: mtval = v; break;
        case 0x344: mip = v; break;
        case 0xB00: mcycle = v; break;
        case 0xB02: minstret = v; break;
        default: break;
    }
}

// trap: save CSRs, redirect to mtvec
static void do_trap(uint32_t cause, uint32_t tval) {
    mepc = pc;
    mcause = cause;
    mtval = tval;
    // push MIE
    uint32_t old_mie = (mstatus >> 3) & 1;
    mstatus = (mstatus & ~0x88) | (old_mie << 7);  // MPIE=old MIE, MIE=0
    pc = mtvec & ~3u;
}

static uint32_t step(uint32_t pass_addr, uint32_t fail_addr) {
    uint32_t insn = rd_mem32(pc);
    uint32_t op = insn & 0x7f;
    uint32_t rd = (insn >> 7) & 31;
    uint32_t f3 = (insn >> 12) & 7;
    uint32_t rs1 = (insn >> 15) & 31;
    uint32_t rs2 = (insn >> 20) & 31;
    uint32_t f7 = (insn >> 25) & 0x7f;
    int64_t a = (int64_t)(int32_t)regs[rs1];
    int64_t b = (int64_t)(int32_t)regs[rs2];
    uint64_t au = (uint64_t)regs[rs1], bu = (uint64_t)regs[rs2];
    uint32_t result = 0;

    switch (op) {
        case 0x33: { // R-type
            if (f7 == 1) { // M ext
                switch (f3) {
                    case 0: result = (uint32_t)(a * b); break;
                    case 1: result = (uint32_t)(((int64_t)a * (int64_t)b) >> 32); break;
                    case 2: result = (uint32_t)(((int64_t)a * (uint64_t)bu) >> 32); break; // MULHSU (rs2 unsigned)
                    case 3: result = (uint32_t)(((uint64_t)au * bu) >> 32); break;
                    case 4: // DIV
                        if (b == 0) result = 0xFFFFFFFF;
                        else if (a == INT32_MIN && b == -1) result = (uint32_t)INT32_MIN;
                        else result = (uint32_t)((int32_t)a / (int32_t)b);
                        break;
                    case 5: // DIVU
                        result = (b == 0) ? 0xFFFFFFFF : (uint32_t)(au / bu);
                        break;
                    case 6: // REM
                        if (b == 0) result = (uint32_t)(int32_t)a;
                        else if (a == INT32_MIN && b == -1) result = 0;
                        else result = (uint32_t)((int32_t)a % (int32_t)b);
                        break;
                    case 7: // REMU
                        result = (b == 0) ? (uint32_t)(int32_t)a : (uint32_t)(au % bu);
                        break;
                }
            } else {
                uint32_t x = regs[rs1], y = regs[rs2];
                switch (f3) {
                    case 0: result = (f7 == 0x20) ? x - y : x + y; break;
                    case 1: result = x << (y & 31); break;
                    case 2: result = (int32_t)x < (int32_t)y; break;
                    case 3: result = x < y; break;
                    case 4: result = x ^ y; break;
                    case 5: result = (f7 == 0x20) ? (uint32_t)((int32_t)x >> (y & 31)) : (x >> (y & 31)); break;
                    case 6: result = x | y; break;
                    case 7: result = x & y; break;
                }
            }
            if (rd) regs[rd] = result;
            pc += 4;
            break;
        }
        case 0x13: { // I-type ALU
            uint32_t x = regs[rs1]; int32_t imm = i_imm(insn);
            switch (f3) {
                case 0: result = x + (uint32_t)imm; break;
                case 1: result = x << (rs2 & 31); break;              // shamt in rs2 field
                case 2: result = (int32_t)x < imm; break;
                case 3: result = x < (uint32_t)imm; break;
                case 4: result = x ^ (uint32_t)imm; break;
                case 5: result = (f7 == 0x20) ? (uint32_t)((int32_t)x >> (rs2 & 31)) : (x >> (rs2 & 31)); break;
                case 6: result = x | (uint32_t)imm; break;
                case 7: result = x & (uint32_t)imm; break;
            }
            if (rd) regs[rd] = result;
            pc += 4;
            break;
        }
        case 0x03: { // loads
            uint32_t ea = regs[rs1] + (uint32_t)i_imm(insn);
            uint32_t v;
            switch (f3) {
                case 0: v = (uint32_t)(int32_t)(int8_t)mem[ea]; break;
                case 1: v = (uint32_t)(int32_t)(int16_t)rd_mem16(ea); break;
                case 2: v = rd_mem32(ea); break;
                case 4: v = mem[ea]; break;
                case 5: v = rd_mem16(ea); break;
                default: v = 0; break;
            }
            if (rd) regs[rd] = v;
            pc += 4;
            break;
        }
        case 0x23: { // stores
            uint32_t ea = regs[rs1] + (uint32_t)s_imm(insn);
            uint32_t v = regs[rs2];
            switch (f3) {
                case 0: mem[ea] = v; break;
                case 1: mem[ea]=v; mem[ea+1]=v>>8; break;
                case 2: wr_mem32(ea, v); break;
            }
            pc += 4;
            break;
        }
        case 0x63: { // branches
            int32_t imm = b_imm(insn);
            bool t;
            switch (f3) {
                case 0: t = (a == b); break;
                case 1: t = (a != b); break;
                case 4: t = ((int32_t)a < (int32_t)b); break;
                case 5: t = (a >= b); break;
                case 6: t = (au < bu); break;
                case 7: t = (au >= bu); break;
                default: t = false; break;
            }
            pc = t ? pc + imm : pc + 4;
            break;
        }
        case 0x6f: { // JAL
            int32_t imm = j_imm(insn);
            if (rd) regs[rd] = pc + 4;
            pc = pc + imm;
            break;
        }
        case 0x67: { // JALR
            uint32_t t = pc + 4;
            pc = (regs[rs1] + (uint32_t)i_imm(insn)) & ~1u;
            if (rd) regs[rd] = t;
            break;
        }
        case 0x37: if (rd) regs[rd] = insn & 0xfffff000; pc += 4; break;  // LUI
        case 0x17: if (rd) regs[rd] = pc + (insn & 0xfffff000); pc += 4; break; // AUIPC
        case 0x0f: pc += 4; break;  // FENCE / FENCE.I (nop)
        case 0x73: { // SYSTEM / CSR
            uint32_t csr = insn >> 20;
            if (f3 == 0) { // ecall/ebreak/mret
                uint32_t f12 = csr;
                if (f12 == 0x000) do_trap(11, 0);        // ecall
                else if (f12 == 0x001) do_trap(3, 0);    // ebreak
                else if (f12 == 0x302) {                 // mret
                    mstatus = (mstatus & ~0x8) | (((mstatus >> 7) & 1) << 3); // MIE=MPIE
                    mstatus = (mstatus & ~0x80) | 0x80;  // MPIE=1
                    pc = mepc;
                } else do_trap(2, 0);
                break;
            }
            uint32_t old = csr_read(csr);
            uint32_t src = (f3 & 4) ? rs1 : regs[rs1];
            switch (f3 & 3) {
                case 1: result = src; break;
                case 2: result = old | src; break;
                case 3: result = old & ~src; break;
                default: result = src; break;
            }
            if (rd) regs[rd] = old;
            bool write = ((f3 & 3) == 1) || (src != 0);
            if (write) csr_write(csr, result);
            pc += 4;
            break;
        }
        default:
            do_trap(2, insn);
            break;
    }
    cycles++;
    mcycle = (uint32_t)cycles;
    minstret++;
    return (pc == pass_addr || pc == fail_addr) ? pc : 0;
}

int main(int argc, char** argv) {
    if (argc != 7) {
        fprintf(stderr, "usage: ref_sim <hex> <pass_addr> <fail_addr> <sig_begin> <sig_words> <out>\n");
        return 2;
    }
    uint32_t pass_addr = (uint32_t)strtoul(argv[2], nullptr, 0);
    uint32_t fail_addr = (uint32_t)strtoul(argv[3], nullptr, 0);
    uint32_t sig_begin = (uint32_t)strtoul(argv[4], nullptr, 0);
    uint32_t sig_words = (uint32_t)strtoul(argv[5], nullptr, 0);
    mem.resize(memsize, 0);
    if (!load_hex(argv[1])) return 2;

    pc = 0x100000;              // entry (rvtest_entry_point) is at TEST_BASE
    uint32_t stop = 0;
    for (; cycles < MAX_CYCLES; ) {
        stop = step(pass_addr, fail_addr);
        if (stop) break;
    }
    if (!stop) { fprintf(stderr, "ref_sim: did not reach pass/fail (pc=0x%x cycles=%llu)\n", pc, (unsigned long long)cycles); return 1; }
    if (stop == fail_addr) { fprintf(stderr, "ref_sim: reached FAIL\n"); return 1; }

    FILE* out = fopen(argv[6], "wb");
    if (!out) { fprintf(stderr, "ref_sim: cannot open %s\n", argv[6]); return 2; }
    for (uint32_t i = 0; i < sig_words; i++)
        fwrite(&mem[sig_begin + i*4], 4, 1, out);
    fclose(out);
    fprintf(stderr, "ref_sim: PASS, %u cycles\n", (unsigned)cycles);
    return 0;
}
