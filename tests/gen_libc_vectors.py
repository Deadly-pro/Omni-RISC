#!/usr/bin/env python3
"""gen_libc_vectors.py — seeded case tables for the F1 gates (firmware track).

Writes two files the gate apps include:

    firmware/apps/libc_vectors.h   memory + string cases
    firmware/apps/conv_vectors.h   numeric-conversion cases

The cases are drawn from a seed (printed, and baked into the header), so an
implementation that memorises answers cannot pass.  Two kinds of expectation:

  * memory/string — the semantics are simple, so the *firmware* checks them
    against a local byte-wise reference (see firmware/apps/libc_test.c).
  * strtoul/strtol — the semantics are fiddly (sign, auto base, endptr on
    "no conversion", wraparound vs saturation), so the expectations are
    computed HERE in Python, independently of the C implementation.

Usage:  tests/gen_libc_vectors.py [--seed N] [--out DIR]
Prints: SEED=<n> TOTAL_MEM=<n> TOTAL_CONV=<n>     (the gates parse these)
"""
import argparse
import os
import random

ALPHA = "abcXYZ019 _-+=.,;:/"
N_BUF = 48          # must match the N_BUF emitted into libc_vectors.h


def c_string(s: str) -> str:
    """C literal for an ASCII string from ALPHA (no quotes/backslashes used)."""
    return '"%s"' % s


def c_bytes(bs, width: int, fill: int = 0) -> str:
    vals = list(bs) + [fill] * (width - len(bs))
    return ", ".join("0x%02x" % v for v in vals)


# --------------------------------------------------------------------------
# Python reference for strtoul/strtol (C standard semantics, 32-bit target)
# --------------------------------------------------------------------------
DIGITS = "0123456789abcdefghijklmnopqrstuvwxyz"
ULONG_MAX = 0xFFFFFFFF
LONG_MAX = 0x7FFFFFFF
LONG_MIN = -0x80000000


def strto(s: str, base: int, signed: bool):
    """Returns (value, end_offset, erange) for the C rules.

    Note the two rules that catch most implementations out:
      - if no digits are converted, endptr is the ORIGINAL pointer (offset 0),
        even when whitespace and/or a sign were consumed;
      - for the unsigned function a '-' negates the result modulo 2**32 and is
        NOT an overflow.
    """
    n = len(s)
    i = 0
    while i < n and s[i] in " \t\n\v\f\r":
        i += 1
    neg = False
    if i < n and s[i] in "+-":
        neg = s[i] == "-"
        i += 1
    # "0x"/"0X" is skipped when a hex digit follows (C99 7.20.1.4); for base 0
    # it also *selects* base 16. For base 0 a leading '0' selects octal but is
    # NOT consumed here - the digit loop takes it as an octal digit, which is
    # what makes "08" and "0x" end up with endptr just past the '0'. (C23's
    # "0b" binary prefix is deliberately not supported; see omni/string.h.)
    if base in (0, 16):
        if (i + 2 < n and s[i] == "0" and s[i + 1] in "xX"
                and 0 <= DIGITS.find(s[i + 2].lower()) < 16):
            base, i = 16, i + 2
        elif base == 0:
            base = 8 if (i < n and s[i] == "0") else 10
    start = i
    val = 0
    while i < n:
        d = DIGITS.find(s[i].lower())
        if d < 0 or d >= base:
            break
        val = val * base + d
        i += 1
    if i == start:
        return 0, 0, False            # no conversion: endptr = nptr
    erange = False
    if signed:
        if val > (LONG_MAX + 1 if neg else LONG_MAX):
            erange = True
            val = LONG_MIN if neg else LONG_MAX
        else:
            val = -val if neg else val
    else:
        if val > ULONG_MAX:
            erange = True
            val = ULONG_MAX
        elif neg:
            val = (-val) % (ULONG_MAX + 1)   # well-defined, not an overflow
    return val, i, erange


# --------------------------------------------------------------------------
# case generation
# --------------------------------------------------------------------------
def rand_str(rng, lo=0, hi=24):
    return "".join(rng.choice(ALPHA) for _ in range(rng.randint(lo, hi)))


def conv_cases(rng):
    """(input, base) pairs covering the standard's awkward corners."""
    fixed = [
        ("", 10), ("   ", 10), ("zzz", 10), ("   +", 10), ("-", 10), ("+", 10),
        ("0", 10), ("-0", 10), ("0000", 10), ("  \t42abc", 10),
        ("+17", 10), ("-17", 10), ("2147483647", 10), ("2147483648", 10),
        ("-2147483648", 10), ("-2147483649", 10), ("4294967295", 10),
        ("4294967296", 10), ("-1", 10), ("99999999999999999999", 10),
        ("0x1f", 16), ("0X1F", 16), ("0x", 0), ("1f", 16), ("-ff", 16),
        ("017", 0), ("08", 0), ("0b101", 0), ("101", 2), ("777", 8),
        ("12z", 36), ("zz", 36), ("0x1Fg", 0), ("  0x10  ", 0),
    ]
    out = list(fixed)
    for _ in range(10):                      # random decimal strings
        v = rng.randint(-2 ** 40, 2 ** 40)
        out.append((str(v), 10))
    for _ in range(6):                       # random bases
        b = rng.choice([2, 8, 16, 36])
        digits = "".join(rng.choice(DIGITS[:b]) for _ in range(rng.randint(1, 9)))
        out.append((digits, b))
    return out


def mem_case(rng):
    ln = rng.randint(0, 32)
    src = bytes(rng.randrange(256) for _ in range(ln))
    dst_off = rng.randint(0, ln)             # <= ln keeps memmove overlapping
    # n may exceed the valid source bytes, but never the buffer: dst_off + n
    # must stay inside N_BUF or the *test app* would run off the end.
    n = rng.randint(0, min(ln + 4, N_BUF - dst_off))
    return {
        "src": src, "len": ln, "n": n,
        "dst_off": dst_off,
        "fill": rng.randrange(256),
    }


def mcmp_case(rng):
    """The expectation must describe the first n bytes only: a difference past
    n is invisible to memcmp, and a sign computed from the whole array would be
    wrong (and unfair)."""
    ln = rng.randint(0, 32)
    a = bytearray(rng.randrange(256) for _ in range(ln))
    b = bytearray(a)
    n = rng.randint(0, ln)                   # compared prefix
    sign = 0
    if n and rng.random() < 0.75:
        at = rng.randrange(n)                # difference inside the prefix
        delta = rng.randint(1, 255)
        if rng.random() < 0.5:
            b[at] = (a[at] + delta) % 256
        else:
            b[at] = (a[at] - delta) % 256
        sign = -1 if a[at] < b[at] else 1
    return {"a": bytes(a), "b": bytes(b), "n": n, "sign": sign}


def build_libc(seed):
    rng = random.Random(seed ^ 0x5EED)
    mem = [mem_case(rng) for _ in range(12)]
    mcmp = [mcmp_case(rng) for _ in range(12)]

    len_cases = [("", 0), ("a", 1)]
    for _ in range(10):
        s = rand_str(rng)
        len_cases.append((s, len(s)))

    cpy = []
    for _ in range(10):
        s = rand_str(rng, 1, 20)
        cpy.append((s, rng.randint(0, len(s) + 3)))   # n may exceed the string

    cmp_cases = [("", "", 0, 0), ("a", "a", 1, 0), ("", "a", 1, -1)]
    for _ in range(9):
        a = rand_str(rng, 1, 12)
        if rng.random() < 0.5:
            b = a
        else:
            b = a[:rng.randrange(len(a))] + rng.choice(ALPHA.replace(a[0] if a else "a", "")) + a[rng.randrange(len(a)) + 1:]
            b = b[:len(a)] if len(b) > len(a) else b
        n = rng.randint(0, max(len(a), len(b)) + 1)
        pa, pb = a[:n], b[:n]                # only the prefix is compared
        sign = (pa > pb) - (pa < pb)
        cmp_cases.append((a, b, n, sign))

    chr_cases = []
    for _ in range(10):
        s = rand_str(rng, 1, 16)
        if rng.random() < 0.6:
            ch = rng.choice(s)
        else:
            ch = rng.choice([c for c in "qQzZ%" if c not in s])
        chr_cases.append((s, ord(ch), s.find(ch), s.rfind(ch)))
    # strchr/strrchr must also match the terminator when asked for '\0';
    # ch is emitted as an int, so this case is expressible.
    for s in ("", "a", "hello"):
        chr_cases.append((s, 0, len(s), len(s)))

    total = 3 * len(mem) + len(mcmp) + len(len_cases) + 2 * len(cpy) \
        + 2 * len(cmp_cases) + 2 * len(chr_cases)
    return mem, mcmp, len_cases, cpy, cmp_cases, chr_cases, total


def build_conv(seed):
    rng = random.Random(seed ^ 0xC0FFEE)
    conv = conv_cases(rng)
    itoa = [(0, "0"), (1, "1"), (-1, "-1"), (9, "9"), (10, "10"),
            (99, "99"), (-99, "-99"), (1000000, "1000000"),
            (2147483647, "2147483647"), (-2147483648, "-2147483648")]
    itoa += [(v, str(v)) for v in (rng.randint(-2 ** 31, 2 ** 31 - 1) for _ in range(6))]
    hex_cases = [(0, "0"), (1, "1"), (15, "f"), (16, "10"), (255, "ff"),
                 (4096, "1000"), (0xDEADBEEF, "deadbeef"), (0xFFFFFFFF, "ffffffff")]
    hex_cases += [(v, "%x" % v) for v in (rng.randrange(2 ** 32) for _ in range(6))]
    total = 2 * len(conv) + len(itoa) + len(hex_cases)
    return conv, itoa, hex_cases, total


# --------------------------------------------------------------------------
# emit
# --------------------------------------------------------------------------
def emit_libc(path, seed, mem, mcmp, len_cases, cpy, cmp_cases, chr_cases, total):
    L = []
    w = L.append
    w("/* generated by tests/gen_libc_vectors.py --seed %d - do not edit */" % seed)
    w("#ifndef LIBC_VECTORS_H")
    w("#define LIBC_VECTORS_H")
    w("#define VEC_SEED %du" % seed)
    w("#define N_BUF 48u")
    w("")
    w("/* ---- memcpy / memmove / memset (same buffers drive all three) ---- */")
    w("typedef struct { unsigned char src[N_BUF]; unsigned int len, n, dst_off;")
    w("                 unsigned char fill; } mem_case_t;")
    w("static const mem_case_t mem_cases[] = {")
    for c in mem:
        w("  { { %s }, %uu, %uu, %uu, 0x%02xu }," % (
            c_bytes(c["src"], 48), c["len"], c["n"], c["dst_off"], c["fill"]))
    w("};")
    w("#define N_MEM_CASES %uu" % len(mem))
    w("")
    w("/* ---- memcmp ---- */")
    w("typedef struct { unsigned char a[N_BUF], b[N_BUF]; unsigned int n; int sign; } mcmp_case_t;")
    w("static const mcmp_case_t mcmp_cases[] = {")
    for c in mcmp:
        w("  { { %s }, { %s }, %uu, %d }," % (
            c_bytes(c["a"], 48), c_bytes(c["b"], 48), c["n"], c["sign"]))
    w("};")
    w("#define N_MCMP_CASES %uu" % len(mcmp))
    w("")
    w("/* ---- strlen ---- */")
    w("typedef struct { const char *s; unsigned int len; } len_case_t;")
    w("static const len_case_t len_cases[] = {")
    for s, n in len_cases:
        w("  { %s, %uu }," % (c_string(s), n))
    w("};")
    w("#define N_LEN_CASES %uu" % len(len_cases))
    w("")
    w("/* ---- strcpy / strncpy ---- */")
    w("typedef struct { const char *src; unsigned int n; } cpy_case_t;")
    w("static const cpy_case_t cpy_cases[] = {")
    for s, n in cpy:
        w("  { %s, %uu }," % (c_string(s), n))
    w("};")
    w("#define N_CPY_CASES %uu" % len(cpy))
    w("")
    w("/* ---- strcmp / strncmp (sign only is specified) ---- */")
    w("typedef struct { const char *a; const char *b; unsigned int n; int sign; } cmp_case_t;")
    w("static const cmp_case_t cmp_cases[] = {")
    for a, b, n, sign in cmp_cases:
        w("  { %s, %s, %uu, %d }," % (c_string(a), c_string(b), n, sign))
    w("};")
    w("#define N_CMP_CASES %uu" % len(cmp_cases))
    w("")
    w("/* ---- strchr / strrchr (-1 = absent) ---- */")
    w("typedef struct { const char *s; int ch; int first, last; } chr_case_t;")
    w("static const chr_case_t chr_cases[] = {")
    for s, ch, first, last in chr_cases:
        w("  { %s, %d, %d, %d }," % (c_string(s), ch, first, last))
    w("};")
    w("#define N_CHR_CASES %uu" % len(chr_cases))
    w("")
    w("#define N_LIBC_CASES %uu" % total)
    w("#endif")
    write(path, "\n".join(L) + "\n")


def emit_conv(path, seed, conv, itoa, hex_cases, total):
    L = []
    w = L.append
    w("/* generated by tests/gen_libc_vectors.py --seed %d - do not edit */" % seed)
    w("#ifndef CONV_VECTORS_H")
    w("#define CONV_VECTORS_H")
    w("")
    w("/* Expectations computed in Python; end_off is the offset from the")
    w("   ORIGINAL pointer (0 == the no-conversion case, endptr == nptr).")
    w("   erange_u/erange_l differ: \"2147483648\" overflows strtol but not")
    w("   strtoul, \"-1\" is a well-defined wraparound for strtoul. */")
    w("typedef struct { const char *s; int base; unsigned long expect_u;")
    w("                 long expect_l; unsigned int end_off;")
    w("                 int erange_u, erange_l; } conv_case_t;")
    w("static const conv_case_t conv_cases[] = {")
    for s, base in conv:
        u, off, ur = strto(s, base, signed=False)
        l, off2, lr = strto(s, base, signed=True)
        assert off == off2, (s, base)
        w("  { %s, %d, %uu, %d, %uu, %d, %d }," % (c_string(s), base, u, l, off, ur, lr))
    w("};")
    w("#define N_CONV_CASES %uu" % len(conv))
    w("")
    w("typedef struct { int v; const char *dec; } itoa_case_t;")
    w("static const itoa_case_t itoa_cases[] = {")
    for v, d in itoa:
        w("  { %d, %s }," % (v, c_string(d)))
    w("};")
    w("#define N_ITOA_CASES %uu" % len(itoa))
    w("")
    w("typedef struct { unsigned long v; const char *hex; } hex_case_t;")
    w("static const hex_case_t hex_cases[] = {")
    for v, h in hex_cases:
        w("  { %uu, %s }," % (v, c_string(h)))
    w("};")
    w("#define N_HEX_CASES %uu" % len(hex_cases))
    w("")
    w("#define N_CONV_TOTAL %uu" % total)
    w("#endif")
    write(path, "\n".join(L) + "\n")


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


# A host C program that cross-checks the Python reference above against the host
# C library. Only where the host's wider types cannot mask a difference: the
# endptr offset for every case, and the value for the cases the 32-bit target
# does not saturate (host strtoul/strtol are 64-bit, so 32-bit saturation is
# ours to get right - see tests/plan notes).  Build:
#   gcc -I<out> -o refcheck refcheck.c && ./refcheck
REFCHECK_C = r"""/* generated by tests/gen_libc_vectors.py --emit-refcheck - host cross-check */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "conv_vectors.h"

int main(void) {
    int bad = 0;
    for (unsigned i = 0; i < N_CONV_CASES; i++) {
        const conv_case_t *c = &conv_cases[i];
        /* omni/string.h deliberately does not implement C23's "0b" binary
           prefix, glibc does. Skip those rather than call the host's answer
           (correct for C23) a mismatch against ours. */
        if (c->base == 0 && (strstr(c->s, "0b") || strstr(c->s, "0B"))) continue;
        char *eu = 0, *el = 0;
        errno = 0;
        unsigned long hu = strtoul(c->s, &eu, c->base);
        int hu_er = (errno == ERANGE);
        errno = 0;
        long hl = strtoll(c->s, &el, c->base);
        int hl_er = (errno == ERANGE);
        unsigned ou = (unsigned)(eu - c->s), ol = (unsigned)(el - c->s);
        if (ou != c->end_off || ol != c->end_off) {
            printf("ENDPTR  \"%s\" base=%d host=%u/%u py=%u\n",
                   c->s, c->base, ou, ol, c->end_off);
            bad++;
        }
        if (!c->erange_u && (hu_er || (unsigned int)hu != c->expect_u)) {
            printf("VALUE-U \"%s\" base=%d host=%lu(er=%d) py=%u\n",
                   c->s, c->base, hu, hu_er, c->expect_u);
            bad++;
        }
        if (!c->erange_l && (hl_er || (long)hl != c->expect_l)) {
            printf("VALUE-L \"%s\" base=%d host=%ld(er=%d) py=%ld\n",
                   c->s, c->base, hl, hl_er, c->expect_l);
            bad++;
        }
    }
    printf("refcheck: %s over %u cases (endptr always, values where 32-bit unsaturated)\n",
           bad ? "MISMATCH" : "OK", N_CONV_CASES);
    return bad != 0;
}
"""


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=None,
                    help="omit for a random seed (printed)")
    ap.add_argument("--out", default="firmware/apps")
    ap.add_argument("--emit-refcheck", metavar="FILE", default=None,
                    help="also write a host C program that cross-checks the "
                         "Python strtoul/strtol reference against the host libc")
    args = ap.parse_args()
    seed = args.seed if args.seed is not None else random.randrange(1 << 30)

    mem, mcmp, len_c, cpy, cmp_c, chr_c, total_mem = build_libc(seed)
    conv, itoa, hex_c, total_conv = build_conv(seed)

    emit_libc(os.path.join(args.out, "libc_vectors.h"), seed,
              mem, mcmp, len_c, cpy, cmp_c, chr_c, total_mem)
    emit_conv(os.path.join(args.out, "conv_vectors.h"), seed,
              conv, itoa, hex_c, total_conv)

    if args.emit_refcheck:
        write(args.emit_refcheck, REFCHECK_C)

    print("SEED=%d" % seed)
    print("TOTAL_MEM=%d" % total_mem)
    print("TOTAL_CONV=%d" % total_conv)


if __name__ == "__main__":
    main()
