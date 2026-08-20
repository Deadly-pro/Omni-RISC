// Omni-RISC — deterministic load→load→branch test (hand-assembled).
// Loads a and b from memory, then does an unsigned bgeu (a>=b). Prints the
// outcome. Bypasses the compiler so instruction spacing is fully controlled.
#include "../drivers/uart.h"

static void dump(unsigned v) {
    static const char hex[] = "0123456789abcdef";
    for (int s = 28; s >= 0; s -= 4) uart_putc(hex[(v >> s) & 0xf]);
}

__attribute__((naked)) static unsigned do_branch(void)
{
    /* a=2 at MEM[0], b=0 at MEM[1]. Return 1 if (a >= b) unsigned else 0. */
    __asm__(
        "lw    t0, 0(a0)\n"     /* t0 = a = mem[0] = 2 */
        "lw    t1, 4(a0)\n"     /* t1 = b = mem[1] = 0 */
        "bgeu  t0, t1, 1f\n"    /* if (a>=b) -> 1 */
        "li    a0, 0\n"
        "ret\n"
        "1:\n"
        "li    a0, 1\n"
        "ret\n");
}

__attribute__((naked)) static unsigned do_branch_nop(void)
{
    /* same but with a nop between the two loads */
    __asm__(
        "lw    t0, 0(a0)\n"
        "nop\n"
        "lw    t1, 4(a0)\n"
        "bgeu  t0, t1, 1f\n"
        "li    a0, 0\n"
        "ret\n"
        "1:\n"
        "li    a0, 1\n"
        "ret\n");
}

__attribute__((naked)) static unsigned do_bltu(void)
{
    /* return 1 if (a < b) unsigned: a=2,b=0 -> FALSE -> expect 0 */
    __asm__(
        "lw    t0, 0(a0)\n"
        "lw    t1, 4(a0)\n"
        "bltu  t0, t1, 1f\n"    /* if (a < b) -> 1 */
        "li    a0, 0\n"
        "ret\n"
        "1:\n"
        "li    a0, 1\n"
        "ret\n");
}

__attribute__((naked)) static unsigned do_bgeu_v2(void)
{
    /* return 1 if (b >= a) unsigned: b=0,a=2 -> FALSE -> expect 0 */
    __asm__(
        "lw    t0, 0(a0)\n"     /* t0 = a = 2 */
        "lw    t1, 4(a0)\n"     /* t1 = b = 0  (dist-1, branch rs1) */
        "bgeu  t1, t0, 1f\n"    /* if (b >= a) -> 1 */
        "li    a0, 0\n"
        "ret\n"
        "1:\n"
        "li    a0, 1\n"
        "ret\n");
}

__attribute__((naked)) static unsigned do_branch_rs1_load(void)
{
    /* branch's RS1 comes from the immediately preceding load.
     * t1=load b (dist-1, rs1), t0=load a. bgeu t1,t0: 0>=2 FALSE -> expect 0 */
    __asm__(
        "lw    t0, 0(a0)\n"     /* t0 = a = 2 */
        "lw    t1, 4(a0)\n"     /* t1 = b = 0  (load, dist-1 to branch) */
        "bgeu  t1, t0, 1f\n"    /* b>=a ? 0>=2 FALSE */
        "li    a0, 0\n"
        "ret\n"
        "1:\n"
        "li    a0, 1\n"
        "ret\n");
}

__attribute__((naked)) static unsigned do_branch_regs(void)
{
    /* same comparison, but operands from registers (addi), not loads.
     * 2 >= 0 TRUE -> expect 1 */
    __asm__(
        "li    t0, 2\n"
        "li    t1, 0\n"
        "bgeu  t0, t1, 1f\n"
        "li    a0, 0\n"
        "ret\n"
        "1:\n"
        "li    a0, 1\n"
        "ret\n");
}

static volatile unsigned mem[16];

void app_main(void)
{
    uart_puts("lbt\r\n");
    mem[0]=2;  /* a */
    mem[1]=0;  /* b */

    unsigned r;
    __asm__("addi a0, %0, 0" ::"r"( (unsigned)mem ) : "a0");
    r = do_branch();    dump(r);  /* a>=b: 2>=0 TRUE exp 1 */
    __asm__("addi a0, %0, 0" ::"r"( (unsigned)mem ) : "a0");
    r = do_branch_nop();dump(r);  /* a>=b exp 1 */
    __asm__("addi a0, %0, 0" ::"r"( (unsigned)mem ) : "a0");
    r = do_bltu();      dump(r);  /* a<b: 2<0 FALSE exp 0 */
    __asm__("addi a0, %0, 0" ::"r"( (unsigned)mem ) : "a0");
    r = do_bgeu_v2();   dump(r);  /* b>=a: 0>=2 FALSE exp 0 */
    __asm__("addi a0, %0, 0" ::"r"( (unsigned)mem ) : "a0");
    r = do_branch_rs1_load(); dump(r);  /* b>=a with rs1=a load exp 0 */
    r = do_branch_regs(); dump(r);      /* 2>=0 via registers exp 1 */

    uart_puts("\r\nlbt done\r\n");
    for (;;) { }
}
