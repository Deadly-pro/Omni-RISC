// =============================================================================
// tb_csr_file.cpp — standalone contract for csr_file.v
// =============================================================================
// Drives the CSR bank purely through its ports (no hierarchy peeking):
//   - a WRITE = set the instruction-port inputs for one CSRR* op, then tick().
//   - a READ  = issue CSRRS with rs1=x0 (uimm=0): a read with NO write side
//               effect, sample csr_rdata combinationally (no tick needed).
// Everything the DUT holds is observable this way, so the TB never reaches
// into the model — if a value can't be read back through the port, it doesn't
// exist as far as the contract is concerned.
//
// funct3: 001 RW  010 RS  011 RC  101 RWI  110 RSI  111 RCI
// =============================================================================
#include <cstdio>
#include <cstdint>
#include "Vcsr_file.h"
#include "verilated.h"

// CSR addresses
enum {
    MSTATUS=0x300, MIE=0x304, MTVEC=0x305, MSCRATCH=0x340,
    MEPC=0x341, MCAUSE=0x342, MTVAL=0x343, MIP=0x344,
    MCYCLE=0xB00, MINSTRET=0xB02, MVENDORID=0xF11
};
// funct3 encodings
enum { F_RW=1, F_RS=2, F_RC=3, F_RWI=5, F_RSI=6, F_RCI=7 };

static Vcsr_file* dut;
static int checks=0, fails=0;

static void tick() {
    dut->clk=0; dut->eval();
    dut->clk=1; dut->eval();
}
static void idle() {                 // park the instruction port (no op)
    dut->csr_en=0; dut->trap_taken=0; dut->mret=0;
    dut->csr_funct3=0; dut->csr_uimm=0; dut->csr_wdata=0; dut->csr_addr=0;
}

// combinational read via CSRRS x0 (no write side effect)
static uint32_t rd(uint32_t addr) {
    idle();
    dut->csr_en=1; dut->csr_addr=addr; dut->csr_funct3=F_RS;
    dut->csr_uimm=0; dut->csr_wdata=0;
    dut->eval();
    uint32_t v = dut->csr_rdata;
    idle(); dut->eval();
    return v;
}

// one CSR instruction (register variant: wdata=src, uimm=rs1 field for skip test)
static void csr_op(uint32_t addr, int f3, uint32_t wdata, int rs1_field) {
    idle();
    dut->csr_en=1; dut->csr_addr=addr; dut->csr_funct3=f3;
    dut->csr_wdata=wdata; dut->csr_uimm=rs1_field;
    dut->eval();
    tick();
    idle(); dut->eval();
}
// immediate variant: zimm carried in uimm
static void csr_opi(uint32_t addr, int f3, int zimm) {
    idle();
    dut->csr_en=1; dut->csr_addr=addr; dut->csr_funct3=f3;
    dut->csr_wdata=0; dut->csr_uimm=zimm;
    dut->eval();
    tick();
    idle(); dut->eval();
}

static void chk(uint32_t got, uint32_t exp, const char* why) {
    checks++;
    if (got!=exp) { fails++; printf("FAIL  got 0x%08X exp 0x%08X  (%s)\n", got,exp,why); }
    else          { printf("pass  0x%08X  (%s)\n", got, why); }
}
static void chk_ill(int expect_ill, const char* why) {
    checks++;
    int got = dut->csr_illegal ? 1:0;
    if (got!=expect_ill) { fails++; printf("FAIL  illegal=%d exp %d  (%s)\n",got,expect_ill,why); }
    else                 { printf("pass  illegal=%d  (%s)\n",got,why); }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    dut = new Vcsr_file;

    // reset
    idle(); dut->reset=1; tick(); tick(); dut->reset=0; dut->eval();

    printf("--- reset state ---\n");
    chk(rd(MTVEC), 0, "mtvec 0 after reset");
    chk(rd(MEPC),  0, "mepc 0 after reset");

    printf("\n--- CSRRW / CSRRS / CSRRC (register) ---\n");
    csr_op(MTVEC, F_RW, 0x00001000, /*rs1*/1);
    chk(rd(MTVEC), 0x00001000, "CSRRW mtvec = 0x1000");
    chk(dut->mtvec_o, 0x00001000, "mtvec_o output tracks");

    csr_op(MTVEC, F_RS, 0x0000000F, 1);
    chk(rd(MTVEC), 0x0000100F, "CSRRS mtvec |= 0xF");
    csr_op(MTVEC, F_RC, 0x0000000F, 1);
    chk(rd(MTVEC), 0x00001000, "CSRRC mtvec &= ~0xF");

    // CSRRW returns OLD value into rd (sampled on the op cycle)
    idle(); dut->csr_en=1; dut->csr_addr=MTVEC; dut->csr_funct3=F_RW;
    dut->csr_wdata=0xDEAD; dut->csr_uimm=1; dut->eval();
    chk(dut->csr_rdata, 0x00001000, "CSRRW rdata = OLD value (0x1000)");
    tick(); idle(); dut->eval();
    chk(rd(MTVEC), 0x0000DEAD, "CSRRW committed new value");

    printf("\n--- immediate variants ---\n");
    csr_opi(MSCRATCH, F_RWI, 0x15);
    chk(rd(MSCRATCH), 0x15, "CSRRWI mscratch = zimm 0x15");
    csr_opi(MSCRATCH, F_RSI, 0x02);
    chk(rd(MSCRATCH), 0x17, "CSRRSI mscratch |= 2");
    csr_opi(MSCRATCH, F_RCI, 0x04);
    chk(rd(MSCRATCH), 0x13, "CSRRCI mscratch &= ~4");

    printf("\n--- write-skip: RS/RC with rs1=x0 (uimm=0) must NOT write ---\n");
    csr_op(MTVEC, F_RW, 0x00002000, 1);          // set known value
    csr_op(MTVEC, F_RS, 0xFFFFFFFF, /*rs1=x0*/0); // set-with-x0: skip
    chk(rd(MTVEC), 0x00002000, "CSRRS rs1=x0 skipped the write");
    csr_op(MTVEC, F_RC, 0xFFFFFFFF, 0);           // clear-with-x0: skip
    chk(rd(MTVEC), 0x00002000, "CSRRC rs1=x0 skipped the write");
    // ...but CSRRW with rs1=x0 STILL writes (writes the source, here 0)
    csr_op(MTVEC, F_RW, 0x00000000, 0);
    chk(rd(MTVEC), 0x00000000, "CSRRW rs1=x0 still writes (0)");

    printf("\n--- illegal: RO write + unknown addr ---\n");
    chk(rd(MVENDORID), 0, "mvendorid reads 0");
    // read of a valid CSR: not illegal
    rd(MTVEC); chk_ill(0, "plain read is legal");
    // write to RO mvendorid -> illegal, value unchanged
    idle(); dut->csr_en=1; dut->csr_addr=MVENDORID; dut->csr_funct3=F_RW;
    dut->csr_wdata=0x1234; dut->csr_uimm=1; dut->eval();
    chk_ill(1, "CSRRW to RO mvendorid is illegal");
    tick(); idle(); dut->eval();
    chk(rd(MVENDORID), 0, "RO mvendorid unchanged after illegal write");
    // unknown address -> illegal
    idle(); dut->csr_en=1; dut->csr_addr=0x000; dut->csr_funct3=F_RW;
    dut->csr_wdata=1; dut->csr_uimm=1; dut->eval();
    chk_ill(1, "unknown CSR addr is illegal");
    idle(); dut->eval();

    printf("\n--- trap capture + mstatus push ---\n");
    // arm MIE=1 (bit3) so the push is observable
    csr_op(MSTATUS, F_RW, 0x00000008, 1);
    chk((rd(MSTATUS)>>3)&1, 1, "mstatus.MIE armed to 1");
    // take a trap
    idle();
    dut->trap_taken=1; dut->trap_pc=0x000DEAD0; dut->trap_cause=2; dut->trap_tval=0xBAD;
    dut->eval(); tick(); idle(); dut->eval();
    chk(rd(MEPC),   0x000DEAD0, "mepc  = trapping PC");
    chk(rd(MCAUSE), 2,          "mcause= 2 (illegal instr)");
    chk(rd(MTVAL),  0xBAD,      "mtval = fault value");
    chk(dut->mepc_o, 0x000DEAD0, "mepc_o output tracks");
    uint32_t ms = rd(MSTATUS);
    chk((ms>>3)&1, 0, "trap pushed: MIE -> 0");
    chk((ms>>7)&1, 1, "trap pushed: MPIE -> old MIE (1)");

    printf("\n--- mret pop ---\n");
    idle(); dut->mret=1; dut->eval(); tick(); idle(); dut->eval();
    ms = rd(MSTATUS);
    chk((ms>>3)&1, 1, "mret popped: MIE <- MPIE (1)");
    chk((ms>>7)&1, 1, "mret: MPIE <- 1");

    printf("\n%d/%d checks passed\n", checks-fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
