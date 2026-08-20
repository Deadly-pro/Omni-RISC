/*
 * Omni-RISC APU — minimal freestanding <stdlib.h> for FreeRTOS kernel.
 * The rv32 libc-dev is not installed and the firmware is compiled
 * -ffreestanding, so the kernel's <stdlib.h> include needs this stub.
 */
#ifndef _OMNI_STDLIB_H
#define _OMNI_STDLIB_H

typedef unsigned int size_t;   /* size_t is also defined by <stddef.h> */

extern __attribute__((noreturn)) void abort( void );

#define NULL ((void *)0)

#endif /* _OMNI_STDLIB_H */
