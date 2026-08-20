/*
 * Omni-RISC APU — FreeRTOS bring-up app.
 *
 * Boot (boot/boot.c) sets mtvec to the old timer-only _trap_handler; this app
 * re-points mtvec at the FreeRTOS port's freertos_risc_v_trap_handler, which
 * dispatches on mcause (timer tick vs ecall yield vs exception), then starts
 * the scheduler. UART has already been initialized by boot.c.
 *
 * Phase 1 milestone: scheduler runs, one periodic task prints over UART so a
 * Verilator UART-decoding testbench can observe the tick.
 */
#include "FreeRTOS.h"
#include "task.h"

#include "../drivers/uart.h"

/* The RISC-V port's trap handler (portASM.S). Our boot set mtvec to the old
 * _trap_handler; now we take over as the real OS. */
extern void freertos_risc_v_trap_handler(void);

/* configASSERT / malloc-failure hooks (declared in FreeRTOSConfig.h). */
void vAssertCalled(const char *file, int line)
{
    uart_puts("\r\n[RTOS] ASSERT: ");
    uart_puts(file);
    uart_puts(":");
    /* tiny decimal print of line */
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

static void vTaskPrint(void *pvParameters)
{
    unsigned tick = 0;
    for (;;) {
        ++tick;
        uart_puts("task A tick ");
        char buf[12];
        int i = 0, n = tick;
        if (n == 0) buf[i++] = '0';
        while (n > 0) { buf[i++] = '0' + (n % 10); n /= 10; }
        for (int j = i - 1; j >= 0; j--) uart_putc(buf[j]);
        uart_puts("\r\n");
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

static void vTaskLow(void *pvParameters)
{
    for (;;) {
        uart_puts("task B (lower prio)\r\n");
        vTaskDelay(pdMS_TO_TICKS(1200));
    }
}

static void put_hex(uint32_t v)
{
    /* print v as 8 uppercase hex digits */
    static const char h[] = "0123456789abcdef";
    char b[9];
    for (int i = 7; i >= 0; i--) { b[i] = h[v & 0xf]; v >>= 4; }
    b[8] = 0;
    uart_puts(b);
}

/* scheduler internals (read-only for bring-up) */
extern void * pxCurrentTCB;

void app_main(void)
{
    uart_puts("[RTOS] app_main: setting mtvec\r\n");
    /* Take over the trap vector from boot.c's _trap_handler. */
    __asm volatile("csrw mtvec, %0" ::"r"((uint32_t)freertos_risc_v_trap_handler));

    BaseType_t r1 = xTaskCreate(vTaskPrint, "A", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
    BaseType_t r2 = xTaskCreate(vTaskLow,  "B", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    uart_puts("[RTOS] xTaskCreate A=");
    put_hex((uint32_t)r1);
    uart_puts(" B=");
    put_hex((uint32_t)r2);
    uart_puts(" starting scheduler curTCB=");
    put_hex((uint32_t)pxCurrentTCB);
    uart_puts("\r\n");

    vTaskStartScheduler();

    /* Should never return. */
    uart_puts("[RTOS] SCHEDULER RETURNED (BAD)\r\n");
    for (;;) { }
}
