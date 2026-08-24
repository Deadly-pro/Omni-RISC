/*
 * Omni-RISC APU — FreeRTOS queue validation app (R1a).
 *
 * Producer (prio 1) sends 1..N_ITEMS into a queue; consumer (prio 2) receives
 * and checks strict ordering. PASS signal for the TB: "QUEUE DONE <n>".
 * Any out-of-order item prints "[Q] ORDER FAIL" and wedges (TB fails).
 */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"

#include "../drivers/uart.h"

extern void freertos_risc_v_trap_handler(void);

#define Q_LEN    8
#define N_ITEMS  1000

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

static QueueHandle_t xQ;

static void vProducer(void *pvParameters)
{
    (void) pvParameters;
    for (uint32_t i = 1; i <= N_ITEMS; i++) {
        xQueueSend(xQ, &i, portMAX_DELAY);
    }
    vTaskDelete(NULL);
}

static void vConsumer(void *pvParameters)
{
    (void) pvParameters;
    uint32_t v, expect = 1;
    for (;;) {
        if (xQueueReceive(xQ, &v, portMAX_DELAY) != pdTRUE)
            continue;
        if (v != expect) {
            uart_puts("[Q] ORDER FAIL got=");
            put_dec(v);
            uart_puts(" want=");
            put_dec(expect);
            uart_puts("\r\n");
            for (;;) { }
        }
        if (expect % 250 == 0) {
            uart_puts("[Q] QUEUE PROGRESS ");
            put_dec(expect);
            uart_puts("\r\n");
        }
        if (expect == N_ITEMS) {
            uart_puts("[Q] QUEUE DONE ");
            put_dec(N_ITEMS);
            uart_puts("\r\n");
            vTaskSuspend(NULL);
        }
        expect++;
    }
}

void app_main(void)
{
    uart_puts("[RTOS-q] app_main: setting mtvec\r\n");
    __asm volatile("csrw mtvec, %0" ::"r"((uint32_t)freertos_risc_v_trap_handler));

    xQ = xQueueCreate(Q_LEN, sizeof(uint32_t));
    BaseType_t rc = pdPASS, rp = pdPASS;
    if (xQ != NULL) {
        /* Consumer higher prio: it blocks on the empty queue, producer fills. */
        rc = xTaskCreate(vConsumer, "CONS", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
        rp = xTaskCreate(vProducer, "PROD", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    }
    uart_puts("[RTOS-q] q=");
    put_dec((uint32_t)(uintptr_t)xQ != 0);
    uart_puts(" cons=");
    put_dec((uint32_t)rc);
    uart_puts(" prod=");
    put_dec((uint32_t)rp);
    uart_puts("\r\n");

    vTaskStartScheduler();

    uart_puts("[RTOS-q] SCHEDULER RETURNED (BAD)\r\n");
    for (;;) { }
}
