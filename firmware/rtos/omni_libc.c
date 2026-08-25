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

__attribute__((weak)) void vApplicationTickHook( void )
{
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
