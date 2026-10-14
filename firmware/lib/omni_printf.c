/* Omni-RISC APU - F2 formatting engine (firmware track).
 *
 * Spec: firmware/include/omni/stdio.h. Approved semantics were cross-checked
 * against the host C library by the gate's host reference run
 * (tests/test_printf.sh -> tests/generated/printf_ref_host.c), which is why the
 * awkward rules below are written the way they are:
 *
 *   - characters are counted even after the buffer is full, so vsnprintf returns
 *     the length that WOULD have been written (the truncation contract);
 *   - at most n bytes are touched, including the terminator, and nothing at all
 *     when n == 0;
 *   - the '0' flag is ignored once a precision is given, '-' beats '0', and '+'
 *     beats the space flag (all three are what the C library does);
 *   - '#' adds 0x/0X only for a non-zero value;
 *   - a precision of 0 with a value of 0 emits no digits at all.
 *
 * 32-bit arithmetic throughout: there is no libgcc in this link, so a 64-bit
 * divide or multiply would become an undefined __udivdi3/__muldi3 reference.
 */
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <omni/stdio.h>
#include "../drivers/uart.h"

#define TMPMAX 24

typedef struct {
    char  *buf;
    size_t n;
    size_t count;
    int   sink;            /* 1 = stream to the UART instead of the buffer */
} out_t;

typedef struct {
    int  left, zero, plus, space, hash;
    int  width, prec;      /* prec < 0 = not given */
} fmt_t;

typedef struct {
    char        body[TMPMAX];   /* digits, after sign/prefix */
    int         blen;
    char        sign;           /* 0, '-', '+', ' ' */
    const char *prefix;         /* "" or "0x"/"0X" */
} field_t;

static void out_ch(out_t *o, char c)
{
    if (o->sink) {
        uart_putc(c);
    } else if (o->buf && o->n && o->count < o->n - 1) {
        o->buf[o->count] = c;
    }
    o->count++;
}

static void out_rep(out_t *o, char c, int k)
{
    while (k-- > 0) out_ch(o, c);
}

static int slen_(const char *s)
{
    int n = 0;
    while (s[n]) n++;
    return n;
}

/* emit sign/prefix/zeros/digits with width and justification */
static void emit_field(out_t *o, const fmt_t *f, const field_t *fd)
{
    int plen = fd->prefix ? slen_(fd->prefix) : 0;
    int total = fd->blen + (fd->sign ? 1 : 0) + plen;
    int pad = (f->width > total) ? f->width - total : 0;
    int zeros = 0;

    if (f->zero && !f->left && f->prec < 0) { zeros = pad; pad = 0; }
    if (!f->left) out_rep(o, ' ', pad);
    if (fd->sign) out_ch(o, fd->sign);
    if (plen) { out_ch(o, fd->prefix[0]); out_ch(o, fd->prefix[1]); }
    out_rep(o, '0', zeros);
    for (int i = 0; i < fd->blen; i++) out_ch(o, fd->body[i]);
    if (f->left) out_rep(o, ' ', pad);
}

/* number -> field, honouring precision (which pads the digits with zeros) */
static void make_number(field_t *fd, unsigned long mag, int base, int upper,
                        const fmt_t *f, char sign, int prefix_when_nonzero)
{
    const char *d = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    char tmp[TMPMAX];
    int  k = 0;

    if (!(f->prec == 0 && mag == 0)) {
        do { tmp[k++] = d[mag % (unsigned long)base]; mag /= (unsigned long)base; } while (mag);
    }
    int zeros = (f->prec > k) ? f->prec - k : 0;
    int i = 0;
    for (int z = 0; z < zeros; z++) fd->body[i++] = '0';
    while (k > 0) fd->body[i++] = tmp[--k];
    fd->blen = i;
    fd->sign = sign;
    fd->prefix = (prefix_when_nonzero && i > 0) ? (upper ? "0X" : "0x") : "";
}

static int do_format(out_t *o, const char *fmt, va_list ap)
{
    for (const char *p = fmt; *p; p++) {
        fmt_t f;
        field_t fd;
        if (*p != '%') { out_ch(o, *p); continue; }
        p++;

        f.left = f.zero = f.plus = f.space = f.hash = 0;
        f.width = 0;
        f.prec = -1;
        for (;; p++) {
            if (*p == '-')      f.left  = 1;
            else if (*p == '0') f.zero  = 1;
            else if (*p == '+') f.plus  = 1;
            else if (*p == ' ') f.space = 1;
            else if (*p == '#') f.hash  = 1;
            else break;
        }
        while (*p >= '0' && *p <= '9') { f.width = f.width * 10 + (*p - '0'); p++; }
        if (*p == '.') {
            p++;
            f.prec = 0;
            while (*p >= '0' && *p <= '9') { f.prec = f.prec * 10 + (*p - '0'); p++; }
        }

        fd.body[0] = 0;
        fd.blen = 0;
        fd.sign = 0;
        fd.prefix = "";

        switch (*p) {
        case 'd':
        case 'i': {
            int v = va_arg(ap, int);
            unsigned long mag;
            char sign;
            if (v < 0) { sign = '-'; mag = (unsigned long)(0u - (unsigned)v); }
            else       { sign = f.plus ? '+' : (f.space ? ' ' : 0); mag = (unsigned long)v; }
            make_number(&fd, mag, 10, 0, &f, sign, 0);
            emit_field(o, &f, &fd);
            break;
        }
        case 'u': {
            unsigned v = va_arg(ap, unsigned);
            make_number(&fd, (unsigned long)v, 10, 0, &f, 0, 0);
            emit_field(o, &f, &fd);
            break;
        }
        case 'x':
        case 'X': {
            unsigned v = va_arg(ap, unsigned);
            int upper = (*p == 'X');
            make_number(&fd, (unsigned long)v, 16, upper, &f, 0, f.hash && v != 0);
            emit_field(o, &f, &fd);
            break;
        }
        case 'c': {
            int c = va_arg(ap, int);
            fd.body[0] = (char)c;
            fd.blen = 1;
            emit_field(o, &f, &fd);
            break;
        }
        case 's': {
            const char *s = va_arg(ap, const char *);
            int len = slen_(s);
            int pad;
            if (f.prec >= 0 && f.prec < len) len = f.prec;
            pad = (f.width > len) ? f.width - len : 0;
            if (!f.left) out_rep(o, ' ', pad);
            for (int i = 0; i < len; i++) out_ch(o, s[i]);
            if (f.left) out_rep(o, ' ', pad);
            break;
        }
        case 'p': {
            unsigned long v = (unsigned long)(uintptr_t)va_arg(ap, void *);
            make_number(&fd, v, 16, 0, &f, 0, 1);
            fd.prefix = "0x";
            emit_field(o, &f, &fd);
            break;
        }
        case '%':
            out_ch(o, '%');
            break;
        default:                       /* unsupported: emit it verbatim */
            out_ch(o, '%');
            if (*p) out_ch(o, *p);
            break;
        }
    }
    return (int)o->count;
}

int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap)
{
    out_t o;
    o.buf = buf;
    o.n = n;
    o.count = 0;
    o.sink = 0;
    int r = do_format(&o, fmt, ap);
    if (n) buf[(o.count < n) ? o.count : n - 1] = '\0';
    return r;
}

int snprintf(char *buf, size_t n, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

int vprintf(const char *fmt, va_list ap)
{
    out_t o;
    o.buf = 0;
    o.n = 0;
    o.count = 0;
    o.sink = 1;
    return do_format(&o, fmt, ap);
}

int printf(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    int r = vprintf(fmt, ap);
    va_end(ap);
    return r;
}
