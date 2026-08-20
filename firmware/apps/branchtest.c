// Omni-RISC APU — branch/compare micro-test (real runtime branches).
// Inputs come from a volatile buffer whose contents the core cannot know at
// compile time, so the compiler MUST emit real B-type branches / sltu. Prints
// results via UART; a testbench can verify each.
#include "../drivers/uart.h"

static void dump(unsigned v) {
    static const char hex[] = "0123456789abcdef";
    for (int s = 28; s >= 0; s -= 4) uart_putc(hex[(v >> s) & 0xf]);
}

static volatile unsigned in[8] = {0, 2, 1, 7, 3, 4, 0xFFFFFFFFu, 1};
static volatile unsigned sink;

void app_main(void)
{
    uart_puts("brt\r\n");
    volatile unsigned a = in[1];   // 2
    volatile unsigned b = in[0];   // 0

    // BLTU: b(0) < a(2) should be TRUE  -> sink = AAAA0001
    sink = 0;
    dump(a); dump(b);   // show the actual loaded values
    if (b < a) sink = 0xAAAA0001u;
    dump(sink);

    // BGEU: a(2) >= b(0) TRUE -> AAAA0002
    sink = 0;
    if (a >= b) sink = 0xAAAA0002u;
    dump(sink);

    // BLT signed: in[6]=0xFFFFFFFF as int(-1) < int(1)=in[7] TRUE -> AAAA0003
    volatile int p = (int)in[6];   // -1
    volatile int q = (int)in[7];   // 1
    sink = 0;
    if (p < q) sink = 0xAAAA0003u;
    dump(sink);

    // unsigned vs signed: in[6](0xFFFFFFFF) >u in[7](1) TRUE -> AAAA0004
    sink = 0;
    if (in[6] > in[7]) sink = 0xAAAA0004u;
    dump(sink);

    // load-use: load mem, branch on loaded value immediately -> AAAA0005
    sink = 0;
    volatile unsigned v = in[3];   // 7
    if (v == 7) sink = 0xAAAA0005u;
    dump(sink);

    // two-load-branch: load a then b, branch on both -> AAAA0006
    volatile unsigned x = in[1];   // 2
    volatile unsigned y = in[0];   // 0
    sink = 0;
    if (y < x) sink = 0xAAAA0006u;   // 0 < 2 TRUE
    dump(sink);
    uart_puts("\r\ndone\r\n");
    for (;;) { }
}
