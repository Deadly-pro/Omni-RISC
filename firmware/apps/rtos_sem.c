/*
 * Omni-RISC APU — FreeRTOS semaphore-from-ISR app (R1b).
 *
 * Exercises the new machine-software-interrupt path end to end:
 *   trigger task (prio 1)  : after 200ms writes CLINT msip (0x0200_0000) = 1
 *   hardware               : msip -> mip.MSIP -> trap, mcause 0x80000003
 *   ISR handler            : clears msip, xSemaphoreGiveFromISR + yield
 *   waiter task (prio 2)   : blocked in xSemaphoreTake, wakes and reports
 *
 * PASS signal for the TB: "[SEM] GOT <n>" then "[SEM] DONE".
 * The handler runs on the port's ISR stack with full context save (portASM.S
 * routes every non-timer interrupt to freertos_risc_v_application_interrupt_handler).
 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "../drivers/uart.h"

extern void freertos_risc_v_trap_handler(void);

#define CLINT_MSIP  (*(volatile uint32_t *)0x02000000u)

static SemaphoreHandle_t xSem;
static volatile uint32_t sem_count = 0;

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

/* Called by portASM.S for every interrupt that is not the mtimer tick. */
void freertos_risc_v_application_interrupt_handler(void)
{
    BaseType_t wakeup = pdFALSE;
    uint32_t mcause;
    __asm volatile("csrr %0, mcause" : "=r"(mcause));
    if (mcause == 0x80000003u) {          /* machine software interrupt */
        CLINT_MSIP = 0;                    /* clear the source first */
        xSemaphoreGiveFromISR(xSem, &wakeup);
        portYIELD_FROM_ISR(wakeup);
    }
    /* other sources: none wired yet — clearing nothing would livelock */
}

static void vWaiterTask(void *pvParameters)
{
    (void) pvParameters;
    for (;;) {
        if (xSemaphoreTake(xSem, portMAX_DELAY) == pdTRUE) {
            sem_count++;
            uart_puts("[SEM] GOT ");
            put_dec(sem_count);
            uart_puts("\r\n");
            if (sem_count >= 3) {
                uart_puts("[SEM] DONE\r\n");
                vTaskSuspend(NULL);
            }
            /* re-arm: ask the trigger task pattern again via a fresh msip
               after a short delay so we test repeated ISR entries */
        }
    }
}

static void vTriggerTask(void *pvParameters)
{
    (void) pvParameters;
    /* fire three times, 200ms apart; waiter counts them */
    for (uint32_t k = 0; k < 3; k++) {
        vTaskDelay(pdMS_TO_TICKS(200));
        CLINT_MSIP = 1;
    }
    vTaskDelete(NULL);
}

void app_main(void)
{
    uart_puts("[SEM] app_main: setting mtvec\r\n");
    __asm volatile("csrw mtvec, %0" ::"r"((uint32_t)freertos_risc_v_trap_handler));
    /* Enable machine software interrupts (mie.MSIE); the port itself only
       turns on MTIE/MEIE. Without this, msip pends but never traps. */
    __asm volatile("csrs mie, %0" ::"r"(0x8));

    xSem = xSemaphoreCreateBinary();
    BaseType_t rw = pdFAIL, rt = pdFAIL;
    if (xSem != NULL) {
        rw = xTaskCreate(vWaiterTask,  "WAIT", configMINIMAL_STACK_SIZE, NULL, 2, NULL);
        rt = xTaskCreate(vTriggerTask, "TRIG", configMINIMAL_STACK_SIZE, NULL, 1, NULL);
    }
    uart_puts("[SEM] sem=");
    put_dec((uint32_t)(uintptr_t)xSem != 0);
    uart_puts(" wait=");
    put_dec((uint32_t)rw);
    uart_puts(" trig=");
    put_dec((uint32_t)rt);
    uart_puts("\r\n");

    vTaskStartScheduler();

    uart_puts("[SEM] SCHEDULER RETURNED (BAD)\r\n");
    for (;;) { }
}
