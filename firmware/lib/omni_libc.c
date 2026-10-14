#include <stdint.h>
#include <stddef.h>
#include <omni/string.h>
/* ------------------------------memory ----------------------------------*/
void  *memcpy (void *dst, const void *src, size_t n){ 
    const uint8_t *cp=(const uint8_t *)src;
    uint8_t *ds=(uint8_t *)dst;
    for(size_t i=0;i<n;i++){
        ds[i]=cp[i];
    }
    return (void *)ds;
}

void  *memmove(void *dst, const void *src, size_t n){
     const uint8_t *cp=(const uint8_t *)src;
     uint8_t *ds=(uint8_t *)dst;
       
    if(dst<src){
        //forward copy
        for(size_t i=0;i<n;i++){
            ds[i]=cp[i];
        }
    }
    else if(dst>src){
        //backward copy
        for(size_t i=n;i>0;i--){
            ds[i-1]=cp[i-1];
        }
    }
    return (void *)ds;
}

void *memset(void *s, int c, size_t n) {
 
    unsigned char *p = (unsigned char *)s;
 
    for (size_t i = 0; i < n; i++) {
        p[i] = (unsigned char)c;
    }
    return s;
}

int    memcmp (const void *a, const void *b, size_t n){
    const uint8_t *c=(const uint8_t *)a;
    const uint8_t *d=(const uint8_t *)b;
    for(size_t i=0;i<n;i++){
        if(d[i]!=c[i])return (int)(c[i] - d[i]);
    }
    return 0;
}

/* ------------------------------strings ----------------------------------*/

size_t strlen(const char *s) {
    size_t i = 0;
    while (s[i] != '\0') { i++;}
    return i;
}

char  *strcpy (char *dst, const char *src){
    size_t i=0;
    while(src[i]!='\0'){
        dst[i]=src[i];
        i++;
    }
    dst[i]='\0';
    return dst;
}

char  *strncpy(char *dst, const char *src, size_t n){
 size_t i=0;
 while(i<n && src[i]!='\0'){
    dst[i]=src[i];
    i++;
 }   
 while(i<n){
    dst[i]='\0';
    i++;
 }
 return dst;
}

int strcmp(const char *a, const char *b) {
    const uint8_t *p1 = (const uint8_t *)a;
    const uint8_t *p2 = (const uint8_t *)b;
    size_t i = 0;
    while (p1[i] == p2[i]) {
        if (p1[i] == '\0') return 0;
        i++;
    }
    return (int)(p1[i] - p2[i]);
}

int    strncmp(const char *a, const char *b, size_t n){
    const uint8_t *p1 = (const uint8_t *)a;
    const uint8_t *p2 = (const uint8_t *)b;
    size_t i = 0;
    while (i < n) {                      /* bound first: never read past n bytes */
        if (p1[i] != p2[i]) return (int)(p1[i] - p2[i]);
        if (p1[i] == '\0') return 0;
        i++;
    }
    return 0;
}

char  *strchr (const char *s, int c){
    size_t i=0;
    while(s[i]!='\0'){
        if(s[i]==(char)c) return (char *)&s[i];
        i++;
    }
    if ((char)c == '\0') {
        return (char *)&s[i];
    }
    return NULL;
}

char  *strrchr(const char *s, int c){
    size_t i=0;
    const char *pt = NULL;
    while(s[i]!='\0'){
        if(s[i]==(char)c) pt = &s[i];
        i++;
    }
    if ((char)c == '\0') {
        return (char *)&s[i];
    }
    return (char *)pt;
}
/* ------------------------------conversion----------------------------------*/

/* Per-call conversion status (see omni/string.h): cleared at the start of every
 * strtoul/strtol call, OMNI_ERANGE when that call saturated, otherwise 0. */
static int conv_errno;

static int digit_of(char c, int base)
{
    int v;
    if (c >= '0' && c <= '9')      v = c - '0';
    else if (c >= 'a' && c <= 'z') v = c - 'a' + 10;
    else if (c >= 'A' && c <= 'Z') v = c - 'A' + 10;
    else return -1;
    return (v < base) ? v : -1;
}

/* Magnitude of the subject sequence, with the two rules that catch most
 * implementations out:
 *   - when no digits are converted, endptr is the ORIGINAL pointer (s), not one
 *     past the whitespace, the sign or the prefix;
 *   - "0x"/"0X" only counts when a hex digit follows it (C99 7.20.1.4), and for
 *     base 0 a leading '0' selects octal WITHOUT being consumed here - the digit
 *     loop takes it, which is what puts endptr just past the '0' in "08" / "0x".
 *
 * 32-bit arithmetic only, on purpose: there is no libgcc in this link, so a
 * 64-bit divide or multiply would be an undefined __udivdi3/__muldi3 reference.
 * Once the value would pass ULONG_MAX the exact magnitude stops mattering, but
 * the remaining digits must still be consumed so endptr lands correctly. */
static unsigned long parse_mag(const char *s, char **endp, int base,
                               int *neg, int *sat)
{
    const char *p = s;
    const char *start;
    unsigned long v = 0;
    int saturate = 0;

    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\v' || *p == '\f' || *p == '\r')
        p++;
    *neg = 0;
    if (*p == '+' || *p == '-') { *neg = (*p == '-'); p++; }

    if (base == 0 || base == 16) {
        if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X') && digit_of(p[2], 16) >= 0) {
            base = 16; p += 2;
        } else if (base == 0) {
            base = (p[0] == '0') ? 8 : 10;
        }
    }
    if (base < 2 || base > 36) {
        *sat = 0;
        if (endp) *endp = (char *)s;
        return 0;
    }

    start = p;
    for (;;) {
        int d = digit_of(*p, base);
        if (d < 0) break;
        if (!saturate) {
            if (v > (0xFFFFFFFFUL - (unsigned long)d) / (unsigned long)base)
                saturate = 1;                       /* keep consuming, drop the value */
            else
                v = v * (unsigned long)base + (unsigned long)d;
        }
        p++;
    }

    if (p == start) {                               /* nothing converted */
        *sat = 0;
        if (endp) *endp = (char *)s;
        return 0;
    }
    *sat = saturate;
    if (endp) *endp = (char *)p;
    return v;
}

unsigned long strtoul(const char *s, char **endp, int base)
{
    int neg, sat;
    conv_errno = 0;
    if (!s) { if (endp) *endp = NULL; return 0; }
    unsigned long v = parse_mag(s, endp, base, &neg, &sat);
    if (sat) {
        conv_errno = OMNI_ERANGE;
        return 0xFFFFFFFFUL;                        /* ULONG_MAX */
    }
    return neg ? (unsigned long)(0u - v) : v;       /* '-' negates modulo 2**32 */
}

long strtol(const char *s, char **endp, int base)
{
    int neg, sat;
    conv_errno = 0;
    if (!s) { if (endp) *endp = NULL; return 0; }
    unsigned long v = parse_mag(s, endp, base, &neg, &sat);
    unsigned long lim = neg ? 2147483648UL : 2147483647UL;
    if (sat || v > lim) {
        conv_errno = OMNI_ERANGE;
        return neg ? (-2147483647L - 1L) : 2147483647L;   /* LONG_MIN / LONG_MAX */
    }
    return neg ? (long)(0u - v) : (long)v;
}

int omni_conv_errno(void)
{
    return conv_errno;
}
/* ------------------------------formatting----------------------------------*/

char *itoa(int v, char *buf) {
    char *p = buf;
    char *p2;
    int negative = 0;
    unsigned int val;

    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    if (v < 0) {
        negative = 1;
        val = (unsigned int)(-v);
    } else {
        val = (unsigned int)v;
    }

    // Extract digits from right to left
    while (val > 0) {
        *p++ = '0' + (val % 10);
        val /= 10;
    }

    if (negative) {
        *p++ = '-';
    }
    *p = '\0';

    // Since we extracted them backwards, reverse the string in-place
    p2 = buf;
    p--;
    while (p2 < p) {
        char temp = *p2;
        *p2 = *p;
        *p = temp;
        p2++;
        p--;
    }

    return buf;
}

char *utoa_hex(unsigned long v, char *buf) {
    char *p = buf;
    char *p2;
    const char hex_chars[] = "0123456789abcdef";

    if (v == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return buf;
    }

    while (v > 0) {
        *p++ = hex_chars[v & 0xF];
        v >>= 4;
    }
    *p = '\0';

    // Reverse the string in-place
    p2 = buf;
    p--;
    while (p2 < p) {
        char temp = *p2;
        *p2 = *p;
        *p = temp;
        p2++;
        p--;
    }

    return buf;
}