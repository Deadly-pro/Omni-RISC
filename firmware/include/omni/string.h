/**
 * @file omni/string.h
 * @brief Firmware-track string/memory interface (F1). The implementation is yours.
 *
 * The signatures marked "C standard" match the C standard exactly, so the
 * compiler's own declarations agree with ours. The conversion helpers carry
 * this project's contract, written out below.
 *
 * Implementation rules:
 *   - freestanding: no libc, only what this header plus <stddef.h>/<stdint.h> give you
 *   - compiled with -fno-builtin (see firmware/Makefile), so no recursion into
 *     compiler builtins - you are testing your code, not gcc's
 *   - no static state, no allocation, no I/O
 *   - on non-RTOS apps these are the only definitions of these names; the RTOS
 *     paths use rtos/omni_libc.c instead (see the Makefile note)
 */
#ifndef OMNI_STRING_H
#define OMNI_STRING_H

#include <stddef.h>

/* ---------------------------------------------------------------- memory --
 * "C standard" semantics, exactly: memcpy may not overlap; memmove MUST
 * handle overlap in both directions; memset writes the low 8 bits of c;
 * memcmp compares as unsigned char and may only promise the SIGN of the
 * difference, not its magnitude.
 */
void  *memcpy (void *dst, const void *src, size_t n);
void  *memmove(void *dst, const void *src, size_t n);
void  *memset (void *s, int c, size_t n);
int    memcmp (const void *a, const void *b, size_t n);

/* --------------------------------------------------------------- strings --
 * strcmp/strncmp return a negative, zero or positive value (sign only is
 * specified). strncpy always writes exactly n bytes, NUL-padding the rest.
 * strchr/strrchr match the terminating NUL when c == 0.
 */
size_t strlen (const char *s);
char  *strcpy (char *dst, const char *src);
char  *strncpy(char *dst, const char *src, size_t n);
int    strcmp (const char *a, const char *b);
int    strncmp(const char *a, const char *b, size_t n);
char  *strchr (const char *s, int c);
char  *strrchr(const char *s, int c);

/* ------------------------------------------------------------ conversion --
 * strtoul/strtol: skip leading whitespace, take at most one '+'/'-', then
 * digits. A `base` of 0 means auto-detect (0x/0X -> 16, leading 0 -> 8, else
 * 10); bases 2..36 are taken literally. For base 16 a "0x"/"0X" prefix is
 * accepted too, but only when a hex digit follows it. C23's "0b"/"0B" binary
 * prefix is NOT supported (a "0b101" converts as octal 0 and stops at 'b').
 * Conversion stops at the first invalid character; if endp is non-NULL it
 * receives that position. Two rules catch most implementations out: when no
 * digits are converted endp is the ORIGINAL pointer (not one past the sign),
 * and a '-' for the unsigned function negates modulo 2**32 and is not an
 * overflow.
 *
 * Overflow: the function saturates (ULONG_MAX / LONG_MAX / LONG_MIN) and
 * reports it through the per-call status below - there is no errno here.
 */
unsigned long strtoul(const char *s, char **endp, int base);
long          strtol (const char *s, char **endp, int base);

/* Per-call conversion status. Cleared at the START of every strtoul/strtol
 * call, set to OMNI_ERANGE if that call saturated, otherwise 0. Not sticky:
 * it always describes the most recent call on this core. */
#define OMNI_ERANGE 34
int  omni_conv_errno(void);

/* ------------------------------------------------------------- formatting --
 * Project-defined (not C standard):
 *   itoa(v, buf)     - decimal, leading '-' when negative, NUL-terminated,
 *                      returns buf. Buffer must be >= 12 bytes (INT_MIN).
 *   utoa_hex(v, buf) - lowercase hex, no "0x" prefix, NUL-terminated,
 *                      returns buf. Buffer must be >= 9 bytes (32-bit value).
 * Neither writes a leading '+' or space padding; printf does that job (F2).
 */
char *itoa(int v, char *buf);
char *utoa_hex(unsigned long v, char *buf);

#endif /* OMNI_STRING_H */
