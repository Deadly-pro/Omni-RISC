/*
 * Omni-RISC APU — FreeRTOS interactive shell (R4).
 *
 * One console task blocks on uart_getchar(), does line editing (backspace,
 * echo) and dispatches commands:
 *   help   — command list
 *   uptime — tick count -> seconds (1000 Hz tick)
 *   ps     — task table via uxTaskGetSystemState (configUSE_TRACE_FACILITY)
 *   ticks  — tick-jitter stats from the tick hook (R1c data, live)
 *   gpu    — Phase-F host-window vector add over MMIO, prints checksum
 *   quit   — prints the [SHELL] QUIT marker; tb_soc_shell stops the sim
 *
 * The tick hook here is the strong definition overriding omni_libc's weak
 * default: it stamps mtime every tick into min/max/count + a last-16 ring.
 */
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>

#include "../drivers/uart.h"
#include "../drivers/gpu.h"

extern void freertos_risc_v_trap_handler(void);

#define MTIME_LO (*(volatile uint32_t *)0x0200BFF8u)

#define LINE_MAX 64

/* ---- number printing ------------------------------------------------- */
static void put_dec(uint32_t v)
{
    char buf[12];
    int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    for (int j = i - 1; j >= 0; j--) uart_putc(buf[j]);
}

/* ---- tick-jitter capture (ISR context via the tick hook) ------------- */
static volatile uint32_t tj_last_lo;
static volatile uint32_t tj_min = 0xFFFFFFFFu, tj_max, tj_n;
static volatile uint32_t tj_ring[16];
static volatile uint32_t tj_ring_idx;

void vApplicationTickHook(void)
{
    uint32_t now = MTIME_LO;          /* deltas are ~50k, low half suffices */
    if (tj_last_lo) {
        uint32_t d = now - tj_last_lo;
        if (d < tj_min) tj_min = d;
        if (d > tj_max) tj_max = d;
        tj_ring[tj_ring_idx & 15u] = d;
        tj_ring_idx++;
        tj_n++;
    }
    tj_last_lo = now;
}

/* ---- FreeRTOS failure hooks (same contract as the other apps) -------- */
void vAssertCalled(const char *file, int line)
{
    uart_puts("\r\n[SHELL] ASSERT: ");
    uart_puts(file);
    uart_putc(':');
    put_dec((uint32_t)line);
    uart_puts("\r\n");
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    uart_puts("\r\n[SHELL] MALLOC FAILED\r\n");
    for (;;) { }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void) xTask;
    uart_puts("\r\n[SHELL] STACK OVERFLOW: ");
    uart_puts(pcTaskName);
    uart_puts("\r\n");
    for (;;) { }
}

/* ---- commands -------------------------------------------------------- */
static void cmd_help(void)
{
    uart_puts("commands: help uptime ps ticks gpu quit\r\n");
    uart_puts("debug:    peek poke mdump gpio mtime heap reboot  (args: hex)\r\n");
}

static void cmd_uptime(void)
{
    TickType_t t = xTaskGetTickCount();
    uart_puts("up ");
    put_dec((uint32_t)(t / configTICK_RATE_HZ));
    uart_putc('.');
    put_dec((uint32_t)(t % configTICK_RATE_HZ));
    uart_puts(" s\r\n");
}

static const char *state_name(eTaskState s)
{
    switch (s) {
    case eRunning:   return "RUN";
    case eReady:     return "RDY";
    case eBlocked:   return "BLK";
    case eSuspended: return "SUS";
    default:         return "DEL";
    }
}

static void cmd_ps(void)
{
    TaskStatus_t st[8];
    UBaseType_t n = uxTaskGetSystemState(st, 8, NULL);
    uart_puts("name       state prio stack(words)\r\n");
    for (UBaseType_t i = 0; i < n; i++) {
        uart_puts(st[i].pcTaskName);
        for (UBaseType_t pad = strlen(st[i].pcTaskName); pad < 11; pad++)
            uart_putc(' ');
        uart_puts(state_name(st[i].eCurrentState));
        uart_puts("  ");
        put_dec((uint32_t)st[i].uxCurrentPriority);
        uart_puts("    ");
        put_dec((uint32_t)st[i].usStackHighWaterMark);
        uart_puts("\r\n");
    }
}

static void cmd_ticks(void)
{
    uart_puts("ticks n=");
    put_dec(tj_n);
    uart_puts(" min=");
    put_dec(tj_min);
    uart_puts(" max=");
    put_dec(tj_max);
    uart_puts(" last:");
    for (uint32_t i = 0; i < 16; i++) {
        uart_putc(' ');
        put_dec(tj_ring[(tj_ring_idx - 1 - i) & 15u]);
    }
    uart_puts("\r\n");
}

#define NVEC 4
static uint32_t ga[NVEC] = { 10, 20, 30, 40 };
static uint32_t gb[NVEC] = { 1, 2, 3, 4 };

static void cmd_gpu(void)
{
    for (unsigned i = 0; i < NVEC; i++) {
        gpu_write_word(i, 0,  ga[i]);   /* lane i word 0  = A[i] */
        gpu_write_word(i, 16, gb[i]);   /* lane i word 16 = B[i] */
    }
    gpu_launch(0);
    gpu_wait_idle();
    uint32_t sum = 0;
    int ok = 1;
    for (unsigned i = 0; i < NVEC; i++) {
        uint32_t c = gpu_read_word(i, 32);
        sum += c;
        ok &= (c == ga[i] + gb[i]);
    }
    uart_puts("gpu sum=");
    put_dec(sum);
    uart_puts(ok ? " PASS\r\n" : " FAIL\r\n");
}

static struct { const char *name; void (*fn)(void); } cmds[] = {
    { "help",   cmd_help },
    { "uptime", cmd_uptime },
    { "ps",     cmd_ps },
    { "ticks",  cmd_ticks },
    { "gpu",    cmd_gpu },
};

/* ---- debug-monitor commands (U-Boot md/mw genre; args are hex) -------- */
static void put_hex32(uint32_t v)
{
    static const char d[] = "0123456789abcdef";
    for (int i = 7; i >= 0; i--) uart_putc(d[(v >> (i * 4)) & 0xF]);
}

/* hex or decimal, optional 0x prefix; returns 1 on success */
static int parse_num(const char *s, uint32_t *out)
{
    uint32_t v = 0;
    if (s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) s += 2;
    if (*s == '\0') return 0;
    for (; *s; s++) {
        char c = *s;
        uint32_t d;
        if (c >= '0' && c <= '9')      d = c - '0';
        else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') d = c - 'A' + 10;
        else return 0;
        v = v * 16 + d;
    }
    *out = v;
    return 1;
}

static char *next_tok(char **cursor)
{
    char *s = *cursor;
    while (*s == ' ') s++;
    if (*s == '\0') return NULL;
    char *tok = s;
    while (*s && *s != ' ') s++;
    if (*s) *s++ = '\0';
    *cursor = s;
    return tok;
}

static void cmd_peek(char *args)
{
    uint32_t addr;
    char *tok = next_tok(&args);
    if (!tok || !parse_num(tok, &addr)) {
        uart_puts("usage: peek <hexaddr>\r\n");
        return;
    }
    uart_puts("0x"); put_hex32(addr);
    uart_puts(": 0x");
    put_hex32(*(volatile uint32_t *)addr);
    uart_puts("\r\n");
}

static void cmd_poke(char *args)
{
    uint32_t addr, val;
    char *tok = next_tok(&args);
    if (!tok || !parse_num(tok, &addr)) {
        uart_puts("usage: poke <hexaddr> <hexval>\r\n");
        return;
    }
    tok = next_tok(&args);
    if (!tok || !parse_num(tok, &val)) {
        uart_puts("usage: poke <hexaddr> <hexval>\r\n");
        return;
    }
    *(volatile uint32_t *)addr = val;
    uart_puts("ok\r\n");
}

static void cmd_mdump(char *args)
{
    uint32_t addr, n;
    char *tok = next_tok(&args);
    if (!tok || !parse_num(tok, &addr)) {
        uart_puts("usage: mdump <hexaddr> <count>\r\n");
        return;
    }
    tok = next_tok(&args);
    if (!tok || !parse_num(tok, &n) || n == 0 || n > 64) {
        uart_puts("usage: mdump <hexaddr> <count 1-64>\r\n");
        return;
    }
    for (uint32_t i = 0; i < n; i++) {
        if ((i & 3) == 0) {
            if (i) uart_puts("\r\n");
            uart_puts("0x"); put_hex32(addr + i * 4); uart_puts(":");
        }
        uart_putc(' ');
        put_hex32(*(volatile uint32_t *)(addr + i * 4));
    }
    uart_puts("\r\n");
}

static void cmd_gpio(char *args)
{
    uint32_t val;
    char *tok = next_tok(&args);
    if (!tok || !parse_num(tok, &val) || val > 0xFF) {
        uart_puts("usage: gpio <hex 00-ff>\r\n");
        return;
    }
    *(volatile uint32_t *)0x40001000u = val;
    uart_puts("gpio = 0x");
    put_hex32(*(volatile uint32_t *)0x40001000u);
    uart_puts("\r\n");
}

static void cmd_mtime(void)
{
    uint32_t hi, lo;
    do {
        hi = *(volatile uint32_t *)0x0200BFFCu;
        lo = *(volatile uint32_t *)0x0200BFF8u;
    } while (hi != *(volatile uint32_t *)0x0200BFFCu);
    uart_puts("mtime = 0x"); put_hex32(hi); put_hex32(lo); uart_puts("\r\n");
}

static void cmd_heap(void)
{
    uart_puts("heap free = ");
    put_dec((uint32_t)xPortGetFreeHeapSize());
    uart_puts(" bytes\r\n");
}

static void cmd_reboot(void)
{
    uart_puts("rebooting...\r\n");
    /* startup.S resets sp and zeroes .bss, so a jump to the reset vector
       is a full re-init (the FreeRTOS image is still in IMEM) */
    for (volatile int i = 0; i < 30000; i++) { }  /* let TX drain + bit times */
    /* csrci mie,0 is a NO-OP (clears zero bits): write zero instead, and park
       mtimecmp at max so a stray MTI can never fire into the un-init boot */
    __asm volatile("csrw mie, x0");
    *(volatile uint32_t *)0x02004004u = 0xFFFFFFFFu;
    *(volatile uint32_t *)0x02004000u = 0xFFFFFFFFu;
    ((void (*)(void))0x00000000u)();
}

/* ---- console task ----------------------------------------------------- */
static void vConsoleTask(void *pv)
{
    (void) pv;
    char line[LINE_MAX];
    uint32_t len = 0;

    uart_puts("[SHELL] ready\r\nomni> ");

    for (;;) {
        int c = uart_getchar();
        if (c < 0) {
            vTaskDelay(pdMS_TO_TICKS(2));
            continue;
        }
        if (c == '\r' || c == '\n') {
            uart_puts("\r\n");
            if (len == 0) {
                uart_puts("omni> ");
                continue;
            }
            line[len] = '\0';
            len = 0;
            if (line[0] != '\0') {
                if (strcmp(line, "quit") == 0) {
                    uart_puts("[SHELL] QUIT\r\n");
                    for (;;) vTaskDelay(pdMS_TO_TICKS(1000));
                }
                /* debug-monitor commands take arguments: split argv-style */
                char *cursor = line;
                char *cmd = next_tok(&cursor);
                if      (strcmp(cmd, "peek")  == 0) cmd_peek(cursor);
                else if (strcmp(cmd, "poke")  == 0) cmd_poke(cursor);
                else if (strcmp(cmd, "mdump") == 0) cmd_mdump(cursor);
                else if (strcmp(cmd, "gpio")  == 0) cmd_gpio(cursor);
                else if (strcmp(cmd, "mtime") == 0) cmd_mtime();
                else if (strcmp(cmd, "heap")  == 0) cmd_heap();
                else if (strcmp(cmd, "reboot") == 0) cmd_reboot();
                else {
                    unsigned k;
                    for (k = 0; k < sizeof(cmds) / sizeof(cmds[0]); k++)
                        if (strcmp(cmd, cmds[k].name) == 0) break;
                    if (k < sizeof(cmds) / sizeof(cmds[0]))
                        cmds[k].fn();
                    else {
                        uart_puts("unknown: ");
                        uart_puts(cmd);
                        uart_puts("\r\n");
                    }
                }
            }
            uart_puts("omni> ");
        } else if (c == 0x7F || c == 0x08) {
            if (len > 0) {
                len--;
                uart_puts("\b \b");
            }
        } else if (c >= 32 && c < 127 && len < LINE_MAX - 1) {
            line[len++] = (char)c;
            uart_putc((char)c);          /* terminal echo */
        }
        /* other control bytes dropped silently */
    }
}

void app_main(void)
{
    uart_init();
    __asm volatile("csrw mtvec, %0" ::"r"((uint32_t)freertos_risc_v_trap_handler));
    __asm volatile("csrs mie, %0" ::"r"(0x8));   /* mie.MSIE (unused, kept uniform) */

    BaseType_t r = xTaskCreate(vConsoleTask, "console",
                               configMINIMAL_STACK_SIZE * 2, NULL, 2, NULL);
    if (r != pdPASS) {
        uart_puts("[SHELL] task create failed\r\n");
        for (;;) { }
    }
    vTaskStartScheduler();
    uart_puts("[SHELL] SCHEDULER RETURNED (BAD)\r\n");
    for (;;) { }
}
