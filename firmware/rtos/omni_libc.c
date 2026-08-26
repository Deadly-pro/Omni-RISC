/*
 * Omni-RISC APU — minimal freestanding libc for the FreeRTOS kernel.
 * Implements the few memory/string functions FreeRTOS references (see
 * firmware/rtos/include/{string,stdlib}.h). Written to compile with
 * -ffreestanding -fno-builtin so the definitions are actually emitted.
 *
 * Also carries a weak vApplicationTickHook default: configUSE_TICK_HOOK is
 * enabled kernel-wide, and apps (e.g. rtos_metrics) override it with a
 * strong definition when they need per-tick instrumentation.
 */
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

__attribute__((weak)) void vApplicationTickHook( void )
{
}

/* Weak default for non-timer interrupts (MSI -> GPU completion, etc.)
   The strong override in shell.c gives the semaphore; apps that don't use
   the GPU interrupt clear msip and return. */
__attribute__((weak)) void freertos_risc_v_application_interrupt_handler( void )
{
    /* clear machine software interrupt pending (CLINT msip) */
    *(volatile uint32_t *)0x02000000u = 0;
}

void *memcpy( void *dest, const void *src, size_t n )
{
    unsigned char *d = ( unsigned char * ) dest;
    const unsigned char *s = ( const unsigned char * ) src;
    while( n-- )
        *d++ = *s++;
    return dest;
}

void *memset( void *s, int c, size_t n )
{
    unsigned char *p = ( unsigned char * ) s;
    while( n-- )
        *p++ = ( unsigned char ) c;
    return s;
}

int strcmp( const char *a, const char *b )
{
    while( *a && ( *a == *b ) ) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

size_t strlen( const char *s )
{
    const char *p = s;
    while( *p )
        p++;
    return ( size_t ) ( p - s );
}

void abort( void )
{
    /* Trap into the machine handler; a good place for a breakpoint. */
    __asm volatile( "ebreak" ::: "memory" );
    for( ;; )
        ;
}
