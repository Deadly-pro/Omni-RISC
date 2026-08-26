// =============================================================================
// tb_gpu_top_kernels.cpp — Phase E5: run the assembled GPU kernels end-to-end
// =============================================================================
// Loads firmware/gpu_kernels/*.hex into imem (each kernel at its own base),
// preloads scratchpad inputs, launches one warp per kernel via PBUS, and
// checks the scratchpad outputs against a C reference.
//   warp0 @ 0x000: vector_add   warp1 @ 0x100: relu
//   warp2 @ 0x200: matmul       warp3 @ 0x300: conv2d
// =============================================================================
#include <cstdio>
#include <cstdint>
#include <cstring>
#include "Vgpu_top.h"
#include "Vgpu_top_gpu_top.h"
#include "Vgpu_top_gpu_fetch.h"
#include "Vgpu_top_exec_lane.h"
#include "Vgpu_top_gpu_scratchpad.h"
#include "verilated.h"

static int checks = 0, fails = 0;
static void check(const char* label, uint32_t expect, uint32_t got) {
    checks++;
    if (expect == got) { printf("  pass  %s\n", label); }
    else { fails++; printf("  FAIL  %s — expected 0x%08X, got 0x%08X\n", label, expect, got); }
}

static Vgpu_top* dut;
static void tick() { dut->clk = 0; dut->eval(); dut->clk = 1; dut->eval(); }
static void pbus_write(uint32_t addr, uint32_t data) {
    dut->pbus_addr = addr; dut->pbus_wdata = data; dut->pbus_wen = 0xF;
    tick();
    dut->pbus_wen = 0;
}

// load a .hex (one 4-digit word per line) into imem at word base
static int load_hex(const char* path, VlUnpacked<SData, 1024>& imem, int base) {
    FILE* f = fopen(path, "r");
    if (!f) { printf("ERROR: cannot open %s\n", path); return -1; }
    unsigned w; int n = 0;
    while (fscanf(f, "%x", &w) == 1) imem[base + n++] = (SData)w;
    fclose(f);
    return n;
}

// per-lane bank access helpers
static uint32_t rd_bank(Vgpu_top_gpu_scratchpad* sp, int lane, int a) {
    switch (lane) {
        case 0: return sp->bank0[a];
        case 1: return sp->bank1[a];
        case 2: return sp->bank2[a];
        default: return sp->bank3[a];
    }
}
static void wr_bank(Vgpu_top_gpu_scratchpad* sp, int lane, int a, uint32_t v) {
    switch (lane) {
        case 0: sp->bank0[a] = v; break;
        case 1: sp->bank1[a] = v; break;
        case 2: sp->bank2[a] = v; break;
        default: sp->bank3[a] = v; break;
    }
}

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    dut = new Vgpu_top;

    // reset (scratchpad reset loop zeroes the banks on this tick)
    dut->reset = 1; tick(); dut->reset = 0;

    auto& imem = dut->gpu_top->u_fetch->imem;
    Vgpu_top_gpu_scratchpad* sp[4] = {
        dut->gpu_top->g_warp__BRA__0__KET____DOT__u_lane->u_sp,
        dut->gpu_top->g_warp__BRA__1__KET____DOT__u_lane->u_sp,
        dut->gpu_top->g_warp__BRA__2__KET____DOT__u_lane->u_sp,
        dut->gpu_top->g_warp__BRA__3__KET____DOT__u_lane->u_sp,
    };

    const char* dir = "../../../firmware/gpu_kernels/";
    char path[256];
    #define HEX(name, base) \
        (snprintf(path, sizeof path, "%s%s.hex", dir, name), load_hex(path, imem, base))
    if (HEX("vector_add", 0x000) < 0) return 2;
    if (HEX("relu",       0x080) < 0) return 2;   // byte 0x100
    if (HEX("matmul",     0x100) < 0) return 2;   // byte 0x200
    if (HEX("conv2d",     0x180) < 0) return 2;   // byte 0x300

    // ---- inputs + references ----
    // vector_add (warp0): A@0..3, B@16..19, C@32..35; lane-varied data
    uint32_t A[4][4], B[4][4];
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 4; i++) {
            A[l][i] = 10*l + i + 1;
            B[l][i] = 100 + 7*l + i;
            wr_bank(sp[0], l, i,      A[l][i]);
            wr_bank(sp[0], l, 16 + i, B[l][i]);
        }

    // relu (warp1): X@0..3 with negatives, Y@16..19
    int32_t X[4][4];
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 4; i++) {
            X[l][i] = (i % 2) ? -(int32_t)(5*l + i) : (int32_t)(3*l + i + 2);
            wr_bank(sp[1], l, i, (uint32_t)X[l][i]);
        }

    // matmul (warp2): A@0..3, B@4..7 row-major 2x2
    uint32_t MA[4][4], MB[4][4];
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 4; i++) {
            MA[l][i] = l + i + 1;
            MB[l][i] = 2*l + i + 1;
            wr_bank(sp[2], l, i,     MA[l][i]);
            wr_bank(sp[2], l, 4 + i, MB[l][i]);
        }

    // conv2d (warp3): X@0..5, W@8..10
    uint32_t CX[4][6], CW[4][3];
    for (int l = 0; l < 4; l++) {
        for (int i = 0; i < 6; i++) { CX[l][i] = l + i + 1; wr_bank(sp[3], l, i, CX[l][i]); }
        for (int k = 0; k < 3; k++) { CW[l][k] = k + 1 + l; wr_bank(sp[3], l, 8 + k, CW[l][k]); }
    }

    // ---- launch all four warps ----
    pbus_write(0x40002000, 0x000);
    pbus_write(0x40002004, 0x100);
    pbus_write(0x40002008, 0x200);
    pbus_write(0x4000200C, 0x300);
    pbus_write(0x40002010, 0x80000000);
    pbus_write(0x40002010, 0x80000001);
    pbus_write(0x40002010, 0x80000002);
    pbus_write(0x40002010, 0x80000003);
    tick();
    check("all 4 warps active", 0xF, dut->active_warps);

    int cyc = 0;
    while (dut->active_warps && cyc < 4000) { tick(); cyc++; }
    printf("  (all warps halted after %d cycles)\n", cyc);
    check("all warps halted", 0, dut->active_warps);

    // ---- verify ----
    char lbl[64];
    // vector_add: C[i] = A[i] + B[i]
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 4; i++) {
            snprintf(lbl, sizeof lbl, "vadd lane%d C[%d]", l, i);
            check(lbl, A[l][i] + B[l][i], rd_bank(sp[0], l, 32 + i));
        }

    // relu: Y[i] = max(X[i], 0)
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 4; i++) {
            uint32_t exp = X[l][i] > 0 ? (uint32_t)X[l][i] : 0;
            snprintf(lbl, sizeof lbl, "relu lane%d Y[%d]", l, i);
            check(lbl, exp, rd_bank(sp[1], l, 16 + i));
        }

    // matmul: C = A*B (2x2)
    for (int l = 0; l < 4; l++) {
        uint32_t c[4] = {
            MA[l][0]*MB[l][0] + MA[l][1]*MB[l][2],
            MA[l][0]*MB[l][1] + MA[l][1]*MB[l][3],
            MA[l][2]*MB[l][0] + MA[l][3]*MB[l][2],
            MA[l][2]*MB[l][1] + MA[l][3]*MB[l][3],
        };
        for (int i = 0; i < 4; i++) {
            snprintf(lbl, sizeof lbl, "matmul lane%d C[%d]", l, i);
            check(lbl, c[i], rd_bank(sp[2], l, 8 + i));
        }
    }

    // conv2d: Y[i] = sum_k X[i+k]*W[k]
    for (int l = 0; l < 4; l++)
        for (int i = 0; i < 4; i++) {
            uint32_t exp = CX[l][i]*CW[l][0] + CX[l][i+1]*CW[l][1] + CX[l][i+2]*CW[l][2];
            snprintf(lbl, sizeof lbl, "conv2d lane%d Y[%d]", l, i);
            check(lbl, exp, rd_bank(sp[3], l, 16 + i));
        }

    // ---- barrier: 4-warp marker sync test ----
    // All warps run the same kernel at byte 0x400 (word base 0x200, clear of
    // conv2d at 0x1BA). Each warp writes PRE 0x55 to its own scratchpad word 0,
    // hits BARRIER, reads word 0 back into word 1 (post-barrier resume proof),
    // and writes POST 0xAA to word 2. Scratchpads are per-warp (no cross-warp
    // shared memory in this GPU), so the test verifies each warp's bank set
    // independently: no deadlock + every marker present.
    if (HEX("barrier", 0x200) < 0) return 2;
    // diag: a lone warp must not deadlock on the barrier (expected count = 1)
    pbus_write(0x40002000, 0x400);
    pbus_write(0x40002010, 0x80000000);
    tick();
    check("barrier diag: 1 warp active", 0x1, dut->active_warps);
    cyc = 0;
    while (dut->active_warps && cyc < 2000) { tick(); cyc++; }
    check("barrier diag: lone warp released (no deadlock)", 0, dut->active_warps);
    printf("  (barrier diag completed after %d cycles)\n", cyc);

    // full sync: all four warps at the barrier
    pbus_write(0x40002000, 0x400);
    pbus_write(0x40002004, 0x400);
    pbus_write(0x40002008, 0x400);
    pbus_write(0x4000200C, 0x400);
    pbus_write(0x40002010, 0x80000000);
    pbus_write(0x40002010, 0x80000001);
    pbus_write(0x40002010, 0x80000002);
    pbus_write(0x40002010, 0x80000003);
    tick();
    check("barrier: 4 warps active", 0xF, dut->active_warps);
    cyc = 0;
    while (dut->active_warps && cyc < 4000) { tick(); cyc++; }
    check("barrier: all warps released (no deadlock)", 0, dut->active_warps);
    printf("  (barrier test completed after %d cycles)\n", cyc);
    // every warp: PRE at word 0, post-barrier load at word 1, POST at word 2
    for (int w = 0; w < 4; w++)
        for (int l = 0; l < 4; l++) {
            snprintf(lbl, sizeof lbl, "barrier warp%d lane%d PRE", w, l);
            check(lbl, 0x55, rd_bank(sp[w], l, 0));
            snprintf(lbl, sizeof lbl, "barrier warp%d lane%d resumed", w, l);
            check(lbl, 0x55, rd_bank(sp[w], l, 1));
            snprintf(lbl, sizeof lbl, "barrier warp%d lane%d POST", w, l);
            check(lbl, 0xAA, rd_bank(sp[w], l, 2));
        }

    printf("\n%d/%d checks passed\n", checks - fails, checks);
    delete dut;
    if (fails) { printf("TB RESULT: FAIL\n"); return 1; }
    printf("TB RESULT: PASS\n");
    return 0;
}
