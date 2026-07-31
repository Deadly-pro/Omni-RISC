// =============================================================================
// tb_cpu_top.cpp — Integration testbench for the Omni-RISC 5-stage CPU
// =============================================================================
// Two independent phases, each a fresh CPU instance loaded with its own
// hand-verified program (assembled with riscv-none-elf-as, encodings embedded):
//
//   PHASE 1 — First boot / control flow
//     Straight-line ALU, JAL link value, taken BEQ, and BOTH wrong-path kills
//     (decode flush + fetch squash). x28-x31 are canaries: nonzero = a squashed
//     instruction escaped and retired.
//
//   PHASE 2 — Memory subsystem (LSU + data_bram + WB load aligner)
//     SW/LW word round trip, LB/LBU/LH/LHU sign- and zero-extension, and
//     byte/half store-lane masking (SB/SH must touch only their lanes, leaving
//     the rest of the word intact). This is the first real exercise of the
//     load/store decode path, EX address gen, and the Option-B split where the
//     raw word rides BRAM->WB and the aligner slices it in WB.
//
// Both phases peek the architectural regfile through the Verilator hierarchy
// (reg_file is marked /* verilator public */).
// =============================================================================

#include <cstdio>
#include <cstdint>
#include "Vcpu_top.h"
#include "Vcpu_top___024root.h"
#include "Vcpu_top_cpu_top.h"
#include "Vcpu_top_decode_stage.h"
#include "Vcpu_top_regfile.h"
#include "Vcpu_top_mem_stage.h"
#include "Vcpu_top_data_bram.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

#ifndef VCD_FILE
#define VCD_FILE "tb_cpu_top.vcd"
#endif

static int checks = 0;
static int fails = 0;

// -----------------------------------------------------------------------------
// A single CPU instance with its own trace. program.hex must exist on disk
// BEFORE the model is constructed — $readmemh runs in instr_bram's initial
// block at time zero.
// -----------------------------------------------------------------------------
struct Sim {
    VerilatedContext* ctx;
    Vcpu_top*         dut;
    VerilatedVcdC*    tfp;
    vluint64_t        t = 0;

    Sim(const char* vcd) {
        // Each instance gets its own context — Verilator refuses to trace two
        // models through one shared (default) context.
        ctx = new VerilatedContext;
        ctx->traceEverOn(true);
        dut = new Vcpu_top(ctx);
        tfp = new VerilatedVcdC;
        dut->trace(tfp, 99);
        tfp->open(vcd);
    }
    ~Sim() { tfp->close(); delete tfp; delete dut; delete ctx; }

    void tick() {
        dut->clk = 0; dut->eval(); tfp->dump(t++);
        dut->clk = 1; dut->eval(); tfp->dump(t++);
    }
    void run(int reset_cyc, int run_cyc) {
        dut->reset = 1;
        for (int i = 0; i < reset_cyc; i++) tick();
        dut->reset = 0;
        for (int i = 0; i < run_cyc; i++) tick();
    }
    uint32_t reg(int idx) {
        // root -> cpu_top -> u_decode -> regfile1 -> reg_file[idx]
        return dut->rootp->cpu_top->u_decode->regfile1->reg_file[idx];
    }
    uint32_t mem(int idx) {
        // root -> cpu_top -> u_mem -> u_dbram -> mem[idx]
        return dut->rootp->cpu_top->u_mem->u_dbram->mem[idx];
    }
};

static void write_hex(const uint32_t* prog, int n) {
    FILE* f = fopen("program.hex", "w");
    if (!f) { printf("ERROR: cannot write program.hex\n"); exit(2); }
    for (int i = 0; i < 1024; i++)
        fprintf(f, "%08X\n", i < n ? prog[i] : 0x00000013u); // NOP fill
    fclose(f);
}

static void check_reg(Sim& s, int idx, uint32_t expect, const char* why) {
    checks++;
    uint32_t got = s.reg(idx);
    if (got != expect) {
        fails++;
        printf("FAIL  x%-2d = 0x%08X, expected 0x%08X  (%s)\n", idx, got, expect, why);
    } else {
        printf("pass  x%-2d = 0x%08X  (%s)\n", idx, got, why);
    }
}

static void check_mem(Sim& s, int idx, uint32_t expect, const char* why) {
    checks++;
    uint32_t got = s.mem(idx);
    if (got != expect) {
        fails++;
        printf("FAIL  mem[0x%03X] = 0x%08X, expected 0x%08X  (%s)\n", idx*4, got, expect, why);
    } else {
        printf("pass  mem[0x%03X] = 0x%08X  (%s)\n", idx*4, got, why);
    }
}

// -----------------------------------------------------------------------------
// PHASE 1 — control flow / first boot
// -----------------------------------------------------------------------------
static void phase_boot() {
    uint32_t prog[64];
    int i = 0;
    prog[i++] = 0x00500093; // 0x00: addi x1, x0, 5
    prog[i++] = 0x00700113; // 0x04: addi x2, x0, 7
    prog[i++] = 0x00000013; // 0x08: nop            (2-NOP rule: no forwarding)
    prog[i++] = 0x00000013; // 0x0C: nop
    prog[i++] = 0x002081B3; // 0x10: add  x3, x1, x2        -> x3 = 12
    prog[i++] = 0x00C002EF; // 0x14: jal  x5, +12           -> 0x20, link = 0x18
    prog[i++] = 0x11100F93; // 0x18: addi x31, x0, 0x111    POISON (decode-flush slot)
    prog[i++] = 0x22200F13; // 0x1C: addi x30, x0, 0x222    POISON (fetch-squash slot)
    prog[i++] = 0x00100313; // 0x20: addi x6, x0, 1         JAL target
    prog[i++] = 0x00000013; // 0x24: nop
    prog[i++] = 0x00000013; // 0x28: nop
    prog[i++] = 0x00108663; // 0x2C: beq  x1, x1, +12       taken -> 0x38
    prog[i++] = 0x33300E93; // 0x30: addi x29, x0, 0x333    POISON (decode-flush slot)
    prog[i++] = 0x44400E13; // 0x34: addi x28, x0, 0x444    POISON (fetch-squash slot)
    prog[i++] = 0x00900393; // 0x38: addi x7, x0, 9         branch target
    write_hex(prog, i);

    Sim s(VCD_FILE);
    s.run(3, 80);

    printf("\n--- PHASE 1: control flow ---\n");
    check_reg(s,  0, 0x00000000, "x0 is hardwired zero");
    check_reg(s,  1, 0x00000005, "addi x1, x0, 5");
    check_reg(s,  2, 0x00000007, "addi x2, x0, 7");
    check_reg(s,  3, 0x0000000C, "add x3, x1, x2 — first-boot check");
    check_reg(s,  5, 0x00000018, "JAL link = pc+4 via WB jump mux");
    check_reg(s,  6, 0x00000001, "instruction at JAL target executed");
    check_reg(s,  7, 0x00000009, "instruction at BEQ target executed");
    check_reg(s, 31, 0x00000000, "JAL wrong-path #1 killed by decode flush");
    check_reg(s, 30, 0x00000000, "JAL wrong-path #2 killed by fetch squash");
    check_reg(s, 29, 0x00000000, "BEQ wrong-path #1 killed by decode flush");
    check_reg(s, 28, 0x00000000, "BEQ wrong-path #2 killed by fetch squash");
}

// -----------------------------------------------------------------------------
// PHASE 2 — memory subsystem (assembled from /tmp/ldst.S, encodings verified)
//   base x1 = 0x100.  Word A = 0x12345678, Word B = 0xFFFFFF80 (-128).
// -----------------------------------------------------------------------------
static void phase_mem() {
    uint32_t prog[64];
    int i = 0;
    prog[i++] = 0x10000093; // li   x1, 0x100          base addr
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x12345137; // lui  x2, 0x12345
    prog[i++] = 0x00000013; // nop   (space lui->addi: no forwarding yet)
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x67810113; // addi x2, x2, 0x678       x2 = 0x12345678
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x0020A023; // sw   x2, 0(x1)           mem[A] = 0x12345678
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x0000A183; // lw   x3, 0(x1)           0x12345678
    prog[i++] = 0x0000C203; // lbu  x4, 0(x1)           0x00000078
    prog[i++] = 0x0030C283; // lbu  x5, 3(x1)           0x00000012
    prog[i++] = 0x0020D303; // lhu  x6, 2(x1)           0x00001234
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0xF8000393; // li   x7, -128            0xFFFFFF80
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x0070A223; // sw   x7, 4(x1)           mem[B] = 0xFFFFFF80
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00408403; // lb   x8,  4(x1)          0xFFFFFF80  (sign)
    prog[i++] = 0x0040C483; // lbu  x9,  4(x1)          0x00000080  (zero)
    prog[i++] = 0x00409503; // lh   x10, 4(x1)          0xFFFFFF80  (sign)
    prog[i++] = 0x0040D583; // lhu  x11, 4(x1)          0x0000FF80  (zero)
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x0AB00613; // li   x12, 0xAB
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00C080A3; // sb   x12, 1(x1)          byte1 lane only
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x0000A683; // lw   x13, 0(x1)          0x1234AB78
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0xFFF00713; // li   x14, -1             0xFFFFFFFF
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00E0A423; // sw   x14, 8(x1)          mem[C] = 0xFFFFFFFF
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x12300793; // li   x15, 0x123
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00F09423; // sh   x15, 8(x1)          low half lane only
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x0080A803; // lw   x16, 8(x1)          0xFFFF0123
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x0000006F; // halt: j halt
    write_hex(prog, i);

    Sim s("tb_cpu_top_mem.vcd");
    s.run(3, 130);

    printf("\n--- PHASE 2: memory subsystem ---\n");
    check_reg(s,  1, 0x00000100, "base address in x1");
    check_reg(s,  2, 0x12345678, "word A built in x2");
    check_reg(s,  3, 0x12345678, "LW  full-word round trip");
    check_reg(s,  4, 0x00000078, "LBU byte0 zero-extend");
    check_reg(s,  5, 0x00000012, "LBU byte3 zero-extend");
    check_reg(s,  6, 0x00001234, "LHU half@2 zero-extend");
    check_reg(s,  7, 0xFFFFFF80, "word B (-128) built in x7");
    check_reg(s,  8, 0xFFFFFF80, "LB  byte 0x80 SIGN-extend");
    check_reg(s,  9, 0x00000080, "LBU byte 0x80 zero-extend");
    check_reg(s, 10, 0xFFFFFF80, "LH  half 0xFF80 SIGN-extend");
    check_reg(s, 11, 0x0000FF80, "LHU half 0xFF80 zero-extend");
    check_reg(s, 13, 0x1234AB78, "SB wrote lane 1 only (rest preserved)");
    check_reg(s, 16, 0xFFFF0123, "SH wrote low half only (high preserved)");
}

// -----------------------------------------------------------------------------
// PHASE 3 — forwarding network (NO NOP crutches; assembled from /tmp/fwd.S)
//   Every dependent instruction sits back-to-back with its producer, so a
//   broken forward path lands the wrong value in the destination register.
//   Load-use is deliberately absent (no hazard unit yet) — this is pure
//   register-to-register EX/MEM and MEM/WB forwarding, plus branch-operand fwd.
// -----------------------------------------------------------------------------
static void phase_forward() {
    uint32_t prog[64];
    int i = 0;
    prog[i++] = 0x00A00093; // 0x00 addi x1,x0,10
    prog[i++] = 0x00108133; // 0x04 add  x2,x1,x1     EX/MEM fwd, both ports  ->20
    prog[i++] = 0x00100193; // 0x08 addi x3,x0,1
    prog[i++] = 0x00118213; // 0x0C addi x4,x3,1       chained d1 ->2
    prog[i++] = 0x00120293; // 0x10 addi x5,x4,1       chained d1 ->3
    prog[i++] = 0x00128313; // 0x14 addi x6,x5,1       chained d1 ->4
    prog[i++] = 0x06400393; // 0x18 addi x7,x0,100
    prog[i++] = 0x00000413; // 0x1C addi x8,x0,0       filler (push x7 to MEM/WB)
    prog[i++] = 0x007384B3; // 0x20 add  x9,x7,x7      MEM/WB fwd, both ports ->200
    prog[i++] = 0x00500513; // 0x24 addi x10,x0,5      older x10 (-> MEM/WB)
    prog[i++] = 0x00900513; // 0x28 addi x10,x0,9      newer x10 (-> EX/MEM)
    prog[i++] = 0x00050593; // 0x2C addi x11,x10,0     priority: nearest wins ->9
    prog[i++] = 0x00700013; // 0x30 addi x0,x0,7       write to x0 discarded
    prog[i++] = 0x00000613; // 0x34 addi x12,x0,0      x0 never forwards ->0
    prog[i++] = 0x00500693; // 0x38 addi x13,x0,5      branch rs1 (d2 at branch)
    prog[i++] = 0x00700713; // 0x3C addi x14,x0,7      branch rs2 (d1 at branch)
    prog[i++] = 0x00E68463; // 0x40 beq  x13,x14,+8    fwd 5!=7 -> NOT taken
    prog[i++] = 0x07B00793; // 0x44 addi x15,x0,123    runs iff branch not taken
    prog[i++] = 0x03700813; // 0x48 addi x16,x0,55     (branch target)
    prog[i++] = 0x0000006F; // 0x4C j halt
    write_hex(prog, i);

    Sim s("tb_cpu_top_fwd.vcd");
    s.run(3, 90);

    printf("\n--- PHASE 3: forwarding network ---\n");
    check_reg(s,  2, 0x00000014, "EX/MEM fwd both ports: add x2,x1,x1 = 20");
    check_reg(s,  4, 0x00000002, "chained d1 fwd: x4 = 2");
    check_reg(s,  5, 0x00000003, "chained d1 fwd: x5 = 3");
    check_reg(s,  6, 0x00000004, "chained d1 fwd: x6 = 4");
    check_reg(s,  9, 0x000000C8, "MEM/WB fwd both ports: add x9,x7,x7 = 200");
    check_reg(s, 11, 0x00000009, "priority: EX/MEM beats MEM/WB, x11 = 9");
    check_reg(s, 12, 0x00000000, "x0 never forwards: x12 = 0 (not 7)");
    check_reg(s, 15, 0x0000007B, "branch read fwd operands: not taken, x15 = 123");
    check_reg(s, 16, 0x00000037, "branch target reached: x16 = 55");
}

// -----------------------------------------------------------------------------
// PHASE 4 — load-use interlock (hazard_unit; assembled from scratch_lduse.S)
//   Every load is immediately followed by a consumer with NO NOP between them.
//   Forwarding alone can't cover this: the load result doesn't exist until WB,
//   so hazard_unit must stall ONE cycle, after which the MEM/WB forward delivers
//   it. Each case also implicitly proves the load's OWN writeback survived the
//   stall (downstream stages must keep draining while IF/ID freezes).
// -----------------------------------------------------------------------------
static void phase_lduse() {
    uint32_t prog[64];
    int i = 0;
    prog[i++] = 0x10000093; // 0x00 addi x1,x0,0x100  base
    prog[i++] = 0x00000013; // 0x04 nop
    prog[i++] = 0x00000013; // 0x08 nop
    prog[i++] = 0x05500113; // 0x0C addi x2,x0,0x55
    prog[i++] = 0x00000013; // 0x10 nop
    prog[i++] = 0x00000013; // 0x14 nop
    prog[i++] = 0x0020A023; // 0x18 sw   x2,0(x1)     mem[0x100]=0x55
    prog[i++] = 0x00000013; // 0x1C nop
    prog[i++] = 0x00000013; // 0x20 nop
    prog[i++] = 0x0000A183; // 0x24 lw   x3,0(x1)     x3<-0x55
    prog[i++] = 0x00118213; // 0x28 addi x4,x3,1      CASE A: stall+fwd -> 0x56
    prog[i++] = 0x00000013; // 0x2C nop
    prog[i++] = 0x00000013; // 0x30 nop
    prog[i++] = 0x0000A283; // 0x34 lw   x5,0(x1)     x5<-0x55
    prog[i++] = 0x00500333; // 0x38 add  x6,x0,x5     CASE B: rs2 dep -> 0x55
    prog[i++] = 0x00000013; // 0x3C nop
    prog[i++] = 0x00000013; // 0x40 nop
    prog[i++] = 0x0000A383; // 0x44 lw   x7,0(x1)     x7<-0x55
    prog[i++] = 0x40138433; // 0x48 sub  x8,x7,x1     CASE C: 0x55-0x100 = 0xFFFFFF55
    prog[i++] = 0x00000013; // 0x4C nop
    prog[i++] = 0x00000013; // 0x50 nop
    prog[i++] = 0x0000A483; // 0x54 lw   x9,0(x1)     x9<-0x55
    prog[i++] = 0x05500513; // 0x58 addi x10,x0,0x55
    prog[i++] = 0x00A48463; // 0x5C beq  x9,x10,+8    CASE D: fwd 0x55==0x55 -> taken
    prog[i++] = 0x0AA00593; // 0x60 addi x11,x0,0xAA  skipped iff taken correctly
    prog[i++] = 0x03300613; // 0x64 addi x12,x0,0x33  branch target
    prog[i++] = 0x0000006F; // 0x68 j halt
    write_hex(prog, i);

    Sim s("tb_cpu_top_lduse.vcd");
    s.run(3, 120);

    printf("\n--- PHASE 4: load-use interlock ---\n");
    check_reg(s,  3, 0x00000055, "CASE A: load x3 wrote back (downstream drained)");
    check_reg(s,  4, 0x00000056, "CASE A: stall+MEM/WB fwd, x4 = x3+1 = 0x56");
    check_reg(s,  5, 0x00000055, "CASE B: load x5 wrote back");
    check_reg(s,  6, 0x00000055, "CASE B: rs2 dep interlock, x6 = x5 = 0x55");
    check_reg(s,  7, 0x00000055, "CASE C: load x7 wrote back");
    check_reg(s,  8, 0xFFFFFF55, "CASE C: sub x8 = 0x55 - 0x100 = 0xFFFFFF55");
    check_reg(s, 11, 0x00000000, "CASE D: branch taken on fwd operand, x11 skipped");
    check_reg(s, 12, 0x00000033, "CASE D: branch target reached, x12 = 0x33");
}

// -----------------------------------------------------------------------------
// PHASE 5 — M-extension multiplier through the live pipeline
//   Proves three things at once: (1) is_mul_div propagates ID->EX->result mux
//   (if it dropped, every mul would writeback the ALU result = garbage);
//   (2) funct3 routes MUL/MULH/MULHSU/MULHU correctly — the x11/x12/x13 trio
//   feeds the SAME operands (0xFFFFFFFF,0xFFFFFFFF) through three funct3 and
//   must produce three different high words; (3) the multiplier reads FORWARDED
//   operands (x6 = mul x5,x5 back-to-back with its producer) and its result
//   forwards out (x4 = x3+1 back-to-back after the mul).
// -----------------------------------------------------------------------------
static void phase_mul() {
    uint32_t prog[64];
    int i = 0;
    prog[i++] = 0x00600093; // 0x00 addi x1,x0,6
    prog[i++] = 0x00700113; // 0x04 addi x2,x0,7
    prog[i++] = 0x00000013; // 0x08 nop
    prog[i++] = 0x00000013; // 0x0C nop
    prog[i++] = 0x022081B3; // 0x10 mul  x3,x1,x2      -> 42 = 0x2A
    prog[i++] = 0x00118213; // 0x14 addi x4,x3,1       back-to-back: fwd MUL result -> 0x2B
    prog[i++] = 0x00000013; // 0x18 nop
    prog[i++] = 0x00000013; // 0x1C nop
    prog[i++] = 0x00500293; // 0x20 addi x5,x0,5
    prog[i++] = 0x02528333; // 0x24 mul  x6,x5,x5      back-to-back: MUL reads fwd x5 -> 25 = 0x19
    prog[i++] = 0x00000013; // 0x28 nop
    prog[i++] = 0x00000013; // 0x2C nop
    prog[i++] = 0x800003B7; // 0x30 lui  x7,0x80000    x7 = 0x80000000
    prog[i++] = 0x00000013; // 0x34 nop
    prog[i++] = 0x00000013; // 0x38 nop
    prog[i++] = 0xFFF00513; // 0x3C addi x10,x0,-1     x10 = 0xFFFFFFFF
    prog[i++] = 0x00000013; // 0x40 nop
    prog[i++] = 0x00000013; // 0x44 nop
    prog[i++] = 0x02A515B3; // 0x48 mulh   x11,x10,x10  ss  high(1)          -> 0x00000000
    prog[i++] = 0x02A53633; // 0x4C mulhu  x12,x10,x10  uu  high(0xFFFFFFFE..) -> 0xFFFFFFFE
    prog[i++] = 0x02A526B3; // 0x50 mulhsu x13,x10,x10  su  high(0xFFFFFFFF..) -> 0xFFFFFFFF
    prog[i++] = 0x00000013; // 0x54 nop
    prog[i++] = 0x00000013; // 0x58 nop
    prog[i++] = 0x02739733; // 0x5C mulh   x14,x7,x7    ss  high(2^62)        -> 0x40000000
    prog[i++] = 0x00000013; // 0x60 nop
    prog[i++] = 0x00000013; // 0x64 nop
    prog[i++] = 0x0000006F; // 0x68 j halt
    write_hex(prog, i);

    Sim s("tb_cpu_top_mul.vcd");
    s.run(3, 100);

    printf("\n--- PHASE 5: M-extension multiplier ---\n");
    check_reg(s,  3, 0x0000002A, "MUL x3 = 6*7 = 42");
    check_reg(s,  4, 0x0000002B, "MUL result forwards: x4 = x3+1 = 0x2B");
    check_reg(s,  6, 0x00000019, "MUL reads fwd operands: x6 = 5*5 = 25");
    check_reg(s, 11, 0x00000000, "MULH  ss: high(-1 * -1) = 0");
    check_reg(s, 12, 0xFFFFFFFE, "MULHU uu: high(0xFFFFFFFF^2) = 0xFFFFFFFE");
    check_reg(s, 13, 0xFFFFFFFF, "MULHSU su: high(-1 * 0xFFFFFFFF) = 0xFFFFFFFF");
    check_reg(s, 14, 0x40000000, "MULH  ss: high(0x80000000^2) = 0x40000000");
}

// -----------------------------------------------------------------------------
// PHASE 6 — M-extension divider through the live pipeline
//   The divider is MULTI-CYCLE, so this phase proves what the single-cycle mul
//   phase could not: (1) the div-busy freeze — fetch + decode hold while EX
//   iterates, and the SAME div op persists in EX/EX the whole time (if the
//   freeze released early, div's rd would be clobbered by div+4 and a garbage
//   result latched); (2) all four ops (DIV/DIVU/REM/REMU) route via funct3 and
//   the sign-magnitude wrapper (signed -20/3, unsigned 0xFFFFFFFF/2); (3) both
//   RISC-V fast paths (divide-by-zero); (4) the div result forwards out the
//   cycle after done (x8 = x7+1 back-to-back); (5) the divisor is read as a
//   FORWARDED operand (x11: divisor x10 produced immediately before the div) —
//   this specifically guards the input-latch: if the DIVIDE loop used the live
//   b_mag instead of the latched b_mag_reg, the drained forwarding source would
//   corrupt the divisor mid-iteration.
// -----------------------------------------------------------------------------
static void phase_div() {
    uint32_t prog[64];
    int i = 0;
    prog[i++] = 0x06400093; // 0x00 addi x1,x0,100
    prog[i++] = 0x00700113; // 0x04 addi x2,x0,7
    prog[i++] = 0x00000013; // 0x08 nop
    prog[i++] = 0x00000013; // 0x0C nop
    prog[i++] = 0x0220C1B3; // 0x10 div  x3,x1,x2      -> 100/7  = 14
    prog[i++] = 0x0220E233; // 0x14 rem  x4,x1,x2      -> 100%7  = 2
    prog[i++] = 0x00000013; // 0x18 nop
    prog[i++] = 0x00000013; // 0x1C nop
    prog[i++] = 0x03200293; // 0x20 addi x5,x0,50
    prog[i++] = 0x00500313; // 0x24 addi x6,x0,5
    prog[i++] = 0x00000013; // 0x28 nop
    prog[i++] = 0x00000013; // 0x2C nop
    prog[i++] = 0x0262C3B3; // 0x30 div  x7,x5,x6      -> 50/5   = 10
    prog[i++] = 0x00138413; // 0x34 addi x8,x7,1       back-to-back: fwd DIV result -> 11
    prog[i++] = 0x00000013; // 0x38 nop
    prog[i++] = 0x00000013; // 0x3C nop
    prog[i++] = 0x05400493; // 0x40 addi x9,x0,84
    prog[i++] = 0x00600513; // 0x44 addi x10,x0,6      divisor produced right before div
    prog[i++] = 0x02A4C5B3; // 0x48 div  x11,x9,x10    FORWARDED divisor -> 84/6 = 14
    prog[i++] = 0x00000013; // 0x4C nop
    prog[i++] = 0x00000013; // 0x50 nop
    prog[i++] = 0xFEC00613; // 0x54 addi x12,x0,-20
    prog[i++] = 0x00300693; // 0x58 addi x13,x0,3
    prog[i++] = 0x00000013; // 0x5C nop
    prog[i++] = 0x00000013; // 0x60 nop
    prog[i++] = 0x02D64733; // 0x64 div  x14,x12,x13   -> -20/3 = -6  (0xFFFFFFFA)
    prog[i++] = 0x02D667B3; // 0x68 rem  x15,x12,x13   -> -20%3 = -2  (0xFFFFFFFE)
    prog[i++] = 0x00000013; // 0x6C nop
    prog[i++] = 0x00000013; // 0x70 nop
    prog[i++] = 0xFFF00A13; // 0x74 addi x20,x0,-1     0xFFFFFFFF
    prog[i++] = 0x00200A93; // 0x78 addi x21,x0,2
    prog[i++] = 0x00000013; // 0x7C nop
    prog[i++] = 0x00000013; // 0x80 nop
    prog[i++] = 0x035A5B33; // 0x84 divu x22,x20,x21   -> 0xFFFFFFFF/2 = 0x7FFFFFFF
    prog[i++] = 0x035A7BB3; // 0x88 remu x23,x20,x21   -> 0xFFFFFFFF%2 = 1
    prog[i++] = 0x00000013; // 0x8C nop
    prog[i++] = 0x00000013; // 0x90 nop
    prog[i++] = 0x02A00813; // 0x94 addi x16,x0,42
    prog[i++] = 0x000008B3; // 0x98 add  x17,x0,x0     x17 = 0 (divisor)
    prog[i++] = 0x00000013; // 0x9C nop
    prog[i++] = 0x00000013; // 0xA0 nop
    prog[i++] = 0x03184933; // 0xA4 div  x18,x16,x17   /0 fast path -> 0xFFFFFFFF
    prog[i++] = 0x031869B3; // 0xA8 rem  x19,x16,x17   /0 fast path -> dividend = 42
    prog[i++] = 0x00000013; // 0xAC nop
    prog[i++] = 0x00000013; // 0xB0 nop
    prog[i++] = 0x0000006F; // 0xB4 j halt
    write_hex(prog, i);

    Sim s("tb_cpu_top_div.vcd");
    s.run(3, 800);

    printf("\n--- PHASE 6: M-extension divider ---\n");
    check_reg(s,  3, 0x0000000E, "DIV  x3 = 100/7 = 14");
    check_reg(s,  4, 0x00000002, "REM  x4 = 100%%7 = 2");
    check_reg(s,  7, 0x0000000A, "DIV  x7 = 50/5 = 10");
    check_reg(s,  8, 0x0000000B, "DIV result forwards: x8 = x7+1 = 11");
    check_reg(s, 11, 0x0000000E, "DIV reads FORWARDED divisor: x11 = 84/6 = 14");
    check_reg(s, 14, 0xFFFFFFFA, "DIV  signed: -20/3 trunc-toward-0 = -6");
    check_reg(s, 15, 0xFFFFFFFE, "REM  signed: -20%%3 takes dividend sign = -2");
    check_reg(s, 22, 0x7FFFFFFF, "DIVU x22 = 0xFFFFFFFF/2 = 0x7FFFFFFF");
    check_reg(s, 23, 0x00000001, "REMU x23 = 0xFFFFFFFF%%2 = 1");
    check_reg(s, 18, 0xFFFFFFFF, "DIV  /0 fast path: x18 = all-ones");
    check_reg(s, 19, 0x0000002A, "REM  /0 fast path: x19 = dividend = 42");
}

// -----------------------------------------------------------------------------
// PHASE 7 — CSR instructions through the live pipeline
//   Proves: (1) is_csr propagates decode->exec and gates csr_file
//   (2) funct3[2] selects register vs immediate source (csr_wdata vs csr_uimm)
//   (3) funct3[1:0] selects RW/RS/RC and write-skip on rs1=x0 (or uimm=0 for I variants)
//   (4) result mux picks csr_rdata over ALU/mul/div results when id_ex_is_csr=1
//   (5) forwarding feeds csr_wdata (fwd_rs1_data) correctly
// -----------------------------------------------------------------------------
static void phase_csr() {
    uint32_t prog[128];
    int i = 0;
    // Setup: x2 = 0x105 (the bit mask used for SET/CLEAR on mtvec)
    prog[i++] = 0x30501273; // 0x00 csrrw x1, mtvec, x0       x1=old mtvec(0), mtvec=0
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x10500113; // 0x14 addi x2, x0, 0x105        x2 = 0x105
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop

    // Register variants on mtvec: set bits, then clear them
    prog[i++] = 0x305121F3; // 0x24 csrrs x3, mtvec, x2       x3=old mtvec(0), mtvec|=0x105
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x30513273; // 0x34 csrrc x4, mtvec, x2       x4=old mtvec(0x105), mtvec&=~0x105=0
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop

    // Immediate variants on mscratch
    prog[i++] = 0x340AD2F3; // 0x50 csrrwi x5, mscratch, 0x15 x5=old mscratch(0), mscratch=0x15
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x34016373; // 0x60 csrrsi x6, mscratch, 2    x6=old mscratch(0x15), mscratch|=2=0x17
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x340273F3; // 0x70 csrrci x7, mscratch, 4    x7=old mscratch(0x17), mscratch&=~4=0x13
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop

    // Re-arm mtvec to 0x105, then prove write-skip semantics
    prog[i++] = 0x30511573; // 0x84 csrrw x10, mtvec, x2      x10=old mtvec(0), mtvec=0x105
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x305025F3; // 0x94 csrrs x11, mtvec, x0      x11=old mtvec(0x105), write SKIPPED
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x30503673; // 0xA4 csrrc x12, mtvec, x0      x12=old mtvec(0x105), write SKIPPED
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x305016F3; // 0xB4 csrrw x13, mtvec, x0      x13=old mtvec(0x105), writes 0
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x30502773; // 0xC4 csrrs x14, mtvec, x0      x14=old mtvec(0) — confirms mtvec=0
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop
    prog[i++] = 0x00000013; // nop

    prog[i++] = 0x0000006F; // j halt
    write_hex(prog, i);

    Sim s("tb_cpu_top_csr.vcd");
    s.run(3, 200);

    printf("\n--- PHASE 7: CSR instructions ---\n");
    check_reg(s,  1, 0x00000000, "CSRRW x1=old mtvec(0), mtvec=0");
    check_reg(s,  3, 0x00000000, "CSRRS x3=old mtvec(0), mtvec|=0x105");
    check_reg(s,  4, 0x00000105, "CSRRC x4=old mtvec(0x105), mtvec&=~0x105=0");
    check_reg(s,  5, 0x00000000, "CSRRWI x5=old mscratch(0), mscratch=0x15");
    check_reg(s,  6, 0x00000015, "CSRRSI x6=old mscratch(0x15), mscratch|=2=0x17");
    check_reg(s,  7, 0x00000017, "CSRRCI x7=old mscratch(0x17), mscratch&=~4=0x13");
    check_reg(s, 10, 0x00000000, "CSRRW x10=old mtvec(0), mtvec re-armed to 0x105");
    check_reg(s, 11, 0x00000105, "CSRRS rs1=x0: x11=old mtvec(0x105), write skipped");
    check_reg(s, 12, 0x00000105, "CSRRC rs1=x0: x12=old mtvec(0x105), write skipped (x11 skipped too)");
    check_reg(s, 13, 0x00000105, "CSRRW rs1=x0: x13=old mtvec(0x105), STILL wrote 0");
    check_reg(s, 14, 0x00000000, "CSRRS x14=old mtvec(0) confirms csrrw wrote 0");
}

// -----------------------------------------------------------------------------
// PHASE 8 — trap_unit through the live pipeline
//   Assembled from /tmp/trap_test.S (riscv64-linux-gnu-as -march=rv32im_zicsr).
//   Four synchronous traps: ecall, ebreak, illegal opcode, illegal CSR write.
//   Each vectors to mtvec (a shared handler) which stores {mepc, mcause,
//   mstatus} into slot [0x200 + trap*16] and advances mepc by 4 before mret.
//   A trap count lives at 0x20C. Verifies: mtvec redirect, mepc capture,
//   cause encoding (11/3/2/2), mstatus MIE push (armed MIE=1 -> handler sees
//   MIE=0, MPIE=1, MPP=M), and that mret returns past the trapping instr.
// -----------------------------------------------------------------------------
static void phase_trap() {
    uint32_t prog[64];
    int i = 0;
    prog[i++] = 0x00000297; // 0x00 auipc t0,0
    prog[i++] = 0x03c28293; // 0x04 addi  t0,t0,60      t0 = &handler (0x3C)
    prog[i++] = 0x30529073; // 0x08 csrw  mtvec,t0
    prog[i++] = 0x00800313; // 0x0C li    t1,8
    prog[i++] = 0x30032073; // 0x10 csrs  mstatus,t1    MIE = 1
    prog[i++] = 0x11100393; // 0x14 li    t2,0x111
    prog[i++] = 0x00000073; // 0x18 ecall               TRAP 0 -> cause 11
    prog[i++] = 0x22200393; // 0x1C li    t2,0x222
    prog[i++] = 0x00100073; // 0x20 ebreak              TRAP 1 -> cause 3
    prog[i++] = 0x33300393; // 0x24 li    t2,0x333
    prog[i++] = 0x0000007f; // 0x28 .word 0x7F          TRAP 2 -> illegal, cause 2
    prog[i++] = 0x44400393; // 0x2C li    t2,0x444
    prog[i++] = 0xf1139073; // 0x30 csrw  mvendorid,t2  TRAP 3 -> RO write, cause 2
    prog[i++] = 0x55500393; // 0x34 li    t2,0x555
    prog[i++] = 0x04c0006f; // 0x38 j     0x84 (done)
    prog[i++] = 0x34102e73; // 0x3C handler: csrr t3,mepc
    prog[i++] = 0x34202ef3; // 0x40 csrr t4,mcause
    prog[i++] = 0x30002f73; // 0x44 csrr t5,mstatus
    prog[i++] = 0x20000f93; // 0x48 li   t6,0x200
    prog[i++] = 0x00cfaf83; // 0x4C lw   t6,12(t6)      count
    prog[i++] = 0x004f9f93; // 0x50 slli t6,t6,4
    prog[i++] = 0x20000293; // 0x54 li   t0,0x200
    prog[i++] = 0x01f282b3; // 0x58 add  t0,t0,t6       slot
    prog[i++] = 0x01c2a023; // 0x5C sw   t3,0(t0)
    prog[i++] = 0x01d2a223; // 0x60 sw   t4,4(t0)
    prog[i++] = 0x01e2a423; // 0x64 sw   t5,8(t0)
    prog[i++] = 0x20000293; // 0x68 li   t0,0x200
    prog[i++] = 0x00c2af83; // 0x6C lw   t6,12(t0)
    prog[i++] = 0x001f8f93; // 0x70 addi t6,t6,1
    prog[i++] = 0x01f2a623; // 0x74 sw   t6,12(t0)
    prog[i++] = 0x004e0e13; // 0x78 addi t3,t3,4
    prog[i++] = 0x341e1073; // 0x7C csrw mepc,t3
    prog[i++] = 0x30200073; // 0x80 mret
    prog[i++] = 0x0000006f; // 0x84 done: j done
    write_hex(prog, i);

    Sim s("tb_cpu_top_trap.vcd");
    s.run(3, 400);

    printf("\n--- PHASE 8: trap_unit (ecall/ebreak/illegal/mret) ---\n");
    check_mem(s, 0x80, 0x00000018, "TRAP0 ecall:  mepc = pc of ecall (0x18)");
    check_mem(s, 0x81, 0x0000000B, "TRAP0 ecall:  mcause = 11");
    check_mem(s, 0x82, 0x00001880, "TRAP0: mstatus MIE pushed (MIE=0, MPIE=1, MPP=M)");
    check_mem(s, 0x84, 0x00000020, "TRAP1 ebreak: mepc = pc of ebreak (0x20)");
    check_mem(s, 0x85, 0x00000003, "TRAP1 ebreak: mcause = 3");
    check_mem(s, 0x86, 0x00001880, "TRAP1: mstatus MIE pushed");
    check_mem(s, 0x88, 0x00000028, "TRAP2 illegal: mepc = pc of 0x7F (0x28)");
    check_mem(s, 0x89, 0x00000002, "TRAP2 illegal: mcause = 2");
    check_mem(s, 0x8A, 0x00001880, "TRAP2: mstatus MIE pushed");
    check_mem(s, 0x8C, 0x00000030, "TRAP3 RO CSR: mepc = pc of csrw (0x30)");
    check_mem(s, 0x8D, 0x00000002, "TRAP3 RO CSR: mcause = 2 (illegal instr)");
    check_mem(s, 0x8E, 0x00001880, "TRAP3: mstatus MIE pushed");
    check_mem(s, 0x83, 0x00000004, "all 4 traps handled (count = 4)");
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    phase_boot();
    phase_mem();
    phase_forward();
    phase_lduse();
    phase_mul();
    phase_div();
    phase_csr();
    phase_trap();

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
