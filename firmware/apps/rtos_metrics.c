/*
 * Omni-RISC APU — FreeRTOS scheduler metrics app (R1c).
 *
 * Measures the three quotable numbers, all stamped with the CLINT's
 * free-running mtime (50k cycles = 1ms tick):
 *
 *   ISR latency   : mtime right before writing CLINT msip vs mtime as the
 *                   first statement of the software-interrupt handler.
 *                   20 samples, 50ms apart (re-armed via msip).
 *   Context switch: two equal-prio tasks ping-pong taskYIELD(); each round
 *                   trip is 2 switches; 200 round trips, min/median/max.
 *   Tick jitter   : vApplicationTickHook stamps mtime every tick; deltas vs
 *                   the ideal 50000 cycles over >=1000 ticks (min/max/dev).
 *
 * PASS signal for the TB: three "[MET] ..." result lines then "[MET] DONE".
 */
#include "FreeRTOS.h"
#include "task.h"

#include "../drivers/uart.h"

extern void freertos_risc_v_trap_handler(void);

#define CLINT_MSIP   (*(volatile uint32_t *)0x02000000u)
#define MTIME_LO     (*(volatile uint32_t *)0x0200BFF8u)
#define MTIME_HI     (*(volatile uint32_t *)0x0200BFFCu)

static uint64_t mtime_now(void)
{
    uint32_t hi, lo;
    do {
        hi = MTIME_HI;
        lo = MTIME_LO;
    } while (hi != MTIME_HI);          /* low half wrapped between reads */
    return ((uint64_t)hi << 32) | lo;
}

void vAssertCalled(const char *file, int line)
{
    uart_puts("\r\n[RTOS] ASSERT: ");
    uart_puts(file);
    uart_puts(":");
    char buf[12];
    int i = 0, n = line;
    if (n == 0) buf[i++] = '0';
    while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
    for (int j = i - 1; j >= 0; j--) uart_putc(buf[j]);
    uart_puts("\r\n");
    for (;;) { }
}

void vApplicationMallocFailedHook(void)
{
    uart_puts("\r\n[RTOS] MALLOC FAILED\r\n");
    for (;;) { }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    (void) xTask;
    uart_puts("\r\n[RTOS] STACK OVERFLOW: ");
    uart_puts(pcTaskName);
    uart_puts("\r\n");
    for (;;) { }
}

static void put_dec(uint32_t v)
{
    char buf[12];
    int i = 0;
    if (v == 0) buf[i++] = '0';
    while (v > 0) { buf[i++] = '0' + (v % 10); v /= 10; }
    for (int j = i - 1; j >= 0; j--) uart_putc(buf[j]);
}
static void puts_line(const char *a, const char *b, uint32_t c,
                      const char *d, uint32_t e, const char *f, uint32_t g)
{
    uart_puts(a); uart_puts(b); put_dec(c);
    uart_puts(d); put_dec(e);
    uart_puts(f); put_dec(g);
    uart_puts("\r\n");
}

/* ---- tick jitter (runs from first tick on) ---- */
static volatile uint64_t tj_last;
static volatile uint32_t tj_min = 0xFFFFFFFFu, tj_max, tj_n;

void vApplicationTickHook(void)
{
    uint64_t now = mtime_now();
    if (tj_last) {
        uint32_t d = (uint32_t)(now - tj_last);
        if (d < tj_min) tj_min = d;
        if (d > tj_max) tj_max = d;
        tj_n++;
    }
    tj_last = now;
}

/* ---- ISR latency samples ---- */
#define LAT_N 20
static volatile uint64_t lat_raise, lat_entry;
static volatile uint32_t lat_count;
static volatile uint32_t lat_min = 0xFFFFFFFFu, lat_max, lat_sum;

void freertos_risc_v_application_interrupt_handler(void)
{
    BaseType_t wakeup = pdFALSE;
    uint32_t mcause;
    __asm volatile("csrr %0, mcause" : "=r"(mcause));
    if (mcause == 0x80000003u) {
        uint32_t d = (uint32_t)(mtime_now() - lat_raise);
        if (d < lat_min) lat_min = d;
        if (d > lat_max) lat_max = d;
        lat_sum += d;
        lat_count++;
        CLINT_MSIP = 0;
        portYIELD_FROM_ISR(wakeup);
    }
}

static void vLatTrigTask(void *pv)
{
    (void) pv;
    /* One sample per iteration: raise, block until the handler logs it.
       Blocking (not spinning) keeps the ecall/interrupt churn low. */
    while (lat_count < LAT_N) {
        vTaskDelay(pdMS_TO_TICKS(50));
        lat_raise = mtime_now();
        CLINT_MSIP = 1;
        uint32_t before = lat_count;
        while (lat_count == before)
            vTaskDelay(pdMS_TO_TICKS(2));
    }
    vTaskSuspend(NULL);
}

/* ---- context-switch ping-pong ---- */
#define SW_TRIPS 200
static volatile uint32_t sw_go, sw_done, sw_tickA, sw_tickB;
static volatile uint32_t sw_min = 0xFFFFFFFFu, sw_max, sw_sum;

static void vYieldA(void *pv)
{
    (void) pv;
    while (!sw_go) vTaskDelay(pdMS_TO_TICKS(10));
    for (uint32_t i = 0; i < SW_TRIPS; i++) {
        uint64_t t0 = mtime_now();
        taskYIELD();                    /* A -> B -> A = 2 switches */
        uint32_t d = (uint32_t)(mtime_now() - t0);
        if (d < sw_min) sw_min = d;
        if (d > sw_max) sw_max = d;
        sw_sum += d;
    }
    sw_done = 1;
    vTaskSuspend(NULL);
}

static void vYieldB(void *pv)
{
    (void) pv;
    while (!sw_go) vTaskDelay(pdMS_TO_TICKS(10));
    while (!sw_done)
        taskYIELD();
    vTaskSuspend(NULL);
}

/* ---- orchestrator ---- */
#define TICK_TARGET 1200

static void vMetricsTask(void *pv)
{
    (void) pv;
    uart_puts("[MET] start\r\n");

    /* phase L: ISR latency */
    xTaskCreate(vLatTrigTask, "LAT", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    while (lat_count < LAT_N) vTaskDelay(pdMS_TO_TICKS(20));
    puts_line("[MET] LAT min=", "", lat_min, " avg=", lat_sum / LAT_N, " max=", lat_max);

    /* phase S: context switch */
    xTaskCreate(vYieldA, "YA", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    xTaskCreate(vYieldB, "YB", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    sw_go = 1;
    while (!sw_done) vTaskDelay(pdMS_TO_TICKS(20));
    puts_line("[MET] SW  min=", "", sw_min, " avg=", sw_sum / SW_TRIPS, " max=", sw_max);

    /* phase T: tick jitter accumulated by the hook since boot */
    while (tj_n < TICK_TARGET) vTaskDelay(pdMS_TO_TICKS(50));
    uint32_t dev = (tj_max > 50000u ? tj_max - 50000u : 50000u - tj_min);
    puts_line("[MET] TICK exp=", "50000 min=", tj_min, " max=", tj_max, " n=", tj_n);
    uart_puts("[MET] DEV ");
    put_dec(dev);
    uart_puts("\r\n");

    uart_puts("[MET] DONE\r\n");
    vTaskSuspend(NULL);
}

void app_main(void)
{
    uart_puts("[MET] app_main: setting mtvec\r\n");
    __asm volatile("csrw mtvec, %0" ::"r"((uint32_t)freertos_risc_v_trap_handler));
    __asm volatile("csrs mie, %0" ::"r"(0x8));   /* mie.MSIE for msip */

    BaseType_t r1 = xTaskCreate(vMetricsTask, "M", configMINIMAL_STACK_SIZE * 2, NULL, 3, NULL);
    uart_puts("[MET] boot=");
    put_dec((uint32_t)r1);
    uart_puts("\r\n");

    vTaskStartScheduler();
    uart_puts("[MET] SCHEDULER RETURNED (BAD)\r\n");
    for (;;) { }
}
