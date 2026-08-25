/*
 * Omni-RISC APU — minimal freestanding <string.h> for FreeRTOS kernel.
 * Provides the handful of string/memory functions the kernel pulls in;
 * implementations live in firmware/rtos/omni_libc.c.
 */
#ifndef _OMNI_STRING_H
#define _OMNI_STRING_H

typedef unsigned int size_t;

void *memcpy( void *dest, const void *src, size_t n );
void *memset( void *s, int c, size_t n );
int strcmp( const char *a, const char *b );
size_t strlen( const char *s );

#endif /* _OMNI_STRING_H */
