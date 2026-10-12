/**
 * @file omni/stdio.h
 * @brief Firmware-track formatting interface (F2). The implementation is yours.
 *
 * Signatures match C99 exactly, so the compiler's own declarations agree with
 * ours. The *supported subset* is narrower than C99 and is written out below -
 * the gate only exercises what is specified here.
 *
 * Implementation rules (same as every phase): freestanding, no libc, no
 * allocation, compiled with -fno-builtin, and your code must not call itself
 * for its own bookkeeping.
 */

#ifndef OMNI_STDIO_H
#define OMNI_STDIO_H

#include <stdarg.h>
#include <stddef.h>

/* ---------------------------------------------------------------------------
 * snprintf / vsnprintf - the real deliverable.
 *
 * Contract (C99 7.19.6.5, which the gate checks byte-exactly against the host
 * C library):
 *
 *   - writes AT MOST n bytes into buf, including the terminating NUL;
 *   - appends a terminating NUL whenever n > 0, even if the output was
 *     truncated;
 *   - writes NOTHING when n == 0 (buf may then be NULL);
 *   - returns the number of characters that WOULD have been written excluding
 *     the terminator - i.e. the truncation is invisible in the return value;
 *   - touches no byte of buf outside [0, n).
 *
 * Supported conversions:
 *
 *   %d %i   int, decimal                 %x %X  unsigned, lowercase/uppercase hex
 *   %u      unsigned, decimal            %c     char (int-argument, promoted)
 *   %s      NUL-terminated string        %p     "0x" + lowercase hex, no leading
 *                                               zeros (glibc-compatible for a
 *                                               non-NULL pointer; NULL is not
 *                                               specified here)
 *   %%      literal '%'
 *
 * Supported flags: '-' (left justify), '0' (zero pad), '+' (always sign),
 * ' ' (space before a non-negative signed value), '#' (hex prefix; ignored for
 * a zero value, as the standard requires).
 * Supported field: decimal width, and '.', then decimal precision.
 *
 * NOT supported (do not rely on it, the gate does not test it): any float
 * conversion (%f %e %g), %o, length modifiers (%l %ll %z %h), '*' width or
 * precision, locale grouping ('), and %n. Behaviour for those is undefined by
 * this header - pick something harmless and document it.
 * ------------------------------------------------------------------------- */

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);
int snprintf (char *buf, size_t n, const char *fmt, ...);

/* Stream the formatted text to the UART (uart_putc per character is fine).
 * Returns the number of characters emitted. */
int vprintf  (const char *fmt, va_list ap);
int printf   (const char *fmt, ...);

#endif /* OMNI_STDIO_H */
