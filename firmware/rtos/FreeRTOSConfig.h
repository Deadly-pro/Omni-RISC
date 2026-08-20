/*
 * Omni-RISC APU — FreeRTOSConfig.h (RV32IM, M-mode, single-hart)
 *
 * Port: portable/GCC/RISC-V (V10.5.1), machine-mode, CLINT timer.
 * Timer map (hardware/rtl/soc/timer.v, base 0x0200_0000):
 *   mtime[63:0]   @ 0x0200_BFF8   (free-running, +1/clk @ 50MHz)
 *   mtimecmp[63:0]@ 0x0200_4000
 * The port's vPortSetupTimerInterrupt arms mtimecmp = mtime + tick; mtip
 * (mip.MTIP, read-only from the SoC CLINT) asserts while mtime >= mtimecmp.
 */
#ifndef FREERTOS_CONFIG_H
#define FREERTOS_CONFIG_H

/* ---- base configuration ---------------------------------------------- */
#define configUSE_PREEMPTION                1
#define configUSE_IDLE_HOOK                 0
#define configUSE_TICK_HOOK                 0
#define configCPU_CLOCK_HZ                  50000000UL
#define configTICK_RATE_HZ                  1000
#define configMAX_PRIORITIES                4
#define configMINIMAL_STACK_SIZE            512         /* words */
#define configTOTAL_HEAP_SIZE               ( 64 * 1024 )
#define configMAX_TASK_NAME_LEN             12
#define configUSE_16_BIT_TICKS              0
#define configIDLE_SHOULD_YIELD             1
#define configUSE_MUTEXES                   1
#define configUSE_RECURSIVE_MUTEXES         0
#define configUSE_COUNTING_SEMAPHORES       1
#define configUSE_TIMERS                    0
#define configQUEUE_REGISTRY_SIZE           0

/* ---- stack / overflow ------------------------------------------------ */
#define configCHECK_FOR_STACK_OVERFLOW      2
#define configISR_STACK_SIZE_WORDS          512
#define configSTACK_DEPTH_TYPE              uint32_t

/* ---- co-routines (off) ---------------------------------------------- */
#define configUSE_CO_ROUTINES               0
#define configMAX_CO_ROUTINE_PRIORITIES     1

/* ---- memory allocation ---------------------------------------------- */
#define configSUPPORT_STATIC_ALLOCATION     0
#define configSUPPORT_DYNAMIC_ALLOCATION    1
#define configUSE_MALLOC_FAILED_HOOK        1

/* ---- RISC-V port ---- -------------------------------------------------- */
/* CLINT timer registers (see header comment / hardware/rtl/soc/timer.v).  */
#define configMTIME_BASE_ADDRESS            ( 0x0200BFF8UL )
#define configMTIMECMP_BASE_ADDRESS         ( 0x02004000UL )
/* Machine mode, no supervisor extension on this core. */
#define configUSE_PORT_OPTIMISED_TASK_SELECTION 0

/* ---- hooks / error -------------------------------------------------- */
#ifndef __ASSEMBLER__
    extern void vAssertCalled( const char * file, int line );
    #define configASSERT( x )   if( ( x ) == 0 ) vAssertCalled( __FILE__, __LINE__ )
    void vApplicationMallocFailedHook( void );
#endif

#define INCLUDE_vTaskPrioritySet            1
#define INCLUDE_uxTaskPriorityGet           1
#define INCLUDE_vTaskDelete                 1
#define INCLUDE_vTaskSuspend                1
#define INCLUDE_vTaskDelayUntil             1
#define INCLUDE_vTaskDelay                  1

#endif /* FREERTOS_CONFIG_H */
