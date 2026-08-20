// Minimal load→branch isolating tests. A vs B disambiguate single vs double load.
#include "../drivers/uart.h"
static void dump(unsigned v) {
    static const char hex[]="0123456789abcdef";
    for (int s=28;s>=0;s-=4) uart_putc(hex[(v>>s)&0xf]);
}
static volatile unsigned mem[16];
void app_main(void){
    uart_puts("z\r\n");
    mem[0]=2; mem[1]=0;
    unsigned r;
    /* A: one load into t0, reg t1=0, bgeu t0,t1 -> 2>=0 true -> 1 */
    r = 0;
    __asm__ volatile (
        "lw t0, 0(%3)\n"
        "li t1, 0\n"
        "bgeu t0, t1, 1f\n"
        "li %0, 0\n"
        "j 2f\n"
        "1:\n"
        "li %0, 1\n"
        "2:\n"
        : "=r"(r) : "r"(0), "r"(0), "r"((unsigned)mem) : "t0","t1","memory");
    dump(r);  /* expect aaaa0001 style: 1 */ uart_puts("A");

    /* B: two loads then bgeu t0,t1 -> 2>=0 true -> 1 (dist-1 load in RS2) */
    r = 0;
    __asm__ volatile (
        "lw t0, 0(%3)\n"
        "lw t1, 4(%3)\n"
        "bgeu t0, t1, 1f\n"
        "li %0, 0\n"
        "j 2f\n"
        "1:\n"
        "li %0, 1\n"
        "2:\n"
        : "=r"(r) : "r"(0), "r"(0), "r"((unsigned)mem) : "t0","t1","memory");
    dump(r);  /* expect 1 */ uart_puts("B");

    /* C: two loads then bgeu t1,t0 -> 0>=2 false -> 0 (dist-1 load in RS1) */
    r = 99;
    __asm__ volatile (
        "lw t0, 0(%3)\n"
        "lw t1, 4(%3)\n"
        "bgeu t1, t0, 1f\n"
        "li %0, 0\n"
        "j 2f\n"
        "1:\n"
        "li %0, 1\n"
        "2:\n"
        : "=r"(r) : "r"(0), "r"(0), "r"((unsigned)mem) : "t0","t1","memory");
    dump(r);  /* expect 0 */ uart_puts("C");

    /* D: EXACT FreeRTOS pattern: bltu t1(0), t0(2) -> 0<2 TRUE -> 1 (dist-1 load in RS1) */
    r = 99;
    __asm__ volatile (
        "lw t0, 0(%3)\n"
        "lw t1, 4(%3)\n"
        "bltu t1, t0, 1f\n"
        "li %0, 0\n"
        "j 2f\n"
        "1:\n"
        "li %0, 1\n"
        "2:\n"
        : "=r"(r) : "r"(0), "r"(0), "r"((unsigned)mem) : "t0","t1","memory");
    dump(r);  /* expect 1 */ uart_puts("D");

    /* E: FULL RTOS chain: load ptr; lw current via ptr; lw new; bltu new,current */
    /* mem[16]=ptr to mem[0]; mem[0]=2 (current); mem[1]=0 (new) */
    mem[16] = (unsigned)&mem[0];
    r = 99;
    __asm__ volatile (
        "lw  t2, 64(%3)\n"     /* t2 = &mem[0] (ptr)                    dist-3 */
        "lw  t0, 0(t2)\n"      /* t0 = current-prio = 2 (load via ptr)  dist-2 */
        "lw  t1, 4(t2)\n"      /* t1 = new-prio = 0                     dist-1 */
        "bltu t1, t0, 1f\n"    /* 0 < 2 TRUE -> 1 */
        "li  %0, 0\n"
        "j   2f\n"
        "1:\n"
        "li  %0, 1\n"
        "2:\n"
        : "=r"(r) : "r"(0), "r"(0), "r"((unsigned)mem) : "t0","t1","t2","memory");
    dump(r);  /* expect 1 */ uart_puts("E");

    uart_puts("\r\nzz\r\n");
    for(;;){}
}
