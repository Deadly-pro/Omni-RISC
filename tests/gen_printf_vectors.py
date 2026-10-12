#!/usr/bin/env python3
"""gen_printf_vectors.py — F2 vector table + host-libc reference runner.

Emits two things:

  <out>/printf_vectors.h    the case tables, shared by the host reference and the
                            firmware gate app so they cannot drift apart
  <emit-host>               a host C program that runs the SAME cases through the
                            HOST C library's snprintf and writes
                              argv[1] = expectations (ret / guard / output bytes)
                              argv[2] = the expected bytes of the stream block

The reference for `snprintf` semantics is therefore the C library itself, not
anyone's reading of the standard. A random batch is drawn from the seed (printed,
overridable with --seed) on top of the hand-written corner cases, so answers
cannot be memorised between runs.

Shared conventions, used identically by both sides (see the emitted header):
  buffer   : PF_BUF bytes, pre-filled with 0xA5 before every call
  written  : (n > 0 && ret >= 0) ? min(ret+1, n) : 0
  guard_ok : every byte in [written, PF_BUF) is still 0xA5  (nothing else touched)
  out[]    : the first min(n, PF_OUT) bytes of the buffer

Usage:  tests/gen_printf_vectors.py [--seed N] [--out DIR] --emit-host FILE
Prints: SEED=<n> N_CASES=<n> N_STREAM=<k>
"""
import argparse
import os
import random

# argument shapes (must match the dispatch in printf_test.c / printf_ref_host.c)
SHAPES = ["I1", "U1", "IU", "S1", "C1", "P1", "I2", "NONE"]

STREAM = [
    # (fmt, shape, a, b, s) - short lines: this block is diffed byte-for-byte
    ("line %d of %d\n", "I2", 3, 7, None),
    ("hex %#06x\n", "U1", 255, 0, None),
    ("str [%-6s]\n", "S1", 0, 0, "ab"),
    ("char %c %%\n", "C1", ord("Z"), 0, None),
    ("limits %d %u\n", "IU", -2147483648, 4294967295, None),
    ("ptr %p\n", "P1", 0xDEAD, 0, None),
]


def fixed_cases():
    """Hand-picked corner cases. Kept inside the subset omni/stdio.h specifies."""
    big = "the quick brown fox jumps over the lazy dog"
    return [
        # --- plain ints, signs, flags, width, precision ---
        ("%d", 64, "I1", 42, 0, None),
        ("%d", 64, "I1", -7, 0, None),
        ("%d", 64, "I1", 0, 0, None),
        ("%i", 64, "I1", -1, 0, None),
        ("%d", 64, "I1", -2147483648, 0, None),   # INT_MIN
        ("%d", 64, "I1", 2147483647, 0, None),    # INT_MAX
        ("%5d", 64, "I1", 42, 0, None),
        ("%5d", 64, "I1", -42, 0, None),
        ("%-5d", 64, "I1", 42, 0, None),
        ("%05d", 64, "I1", 42, 0, None),
        ("%05d", 64, "I1", -42, 0, None),
        ("%+d", 64, "I1", 42, 0, None),
        ("%+d", 64, "I1", -42, 0, None),
        ("% d", 64, "I1", 42, 0, None),
        ("% d", 64, "I1", -42, 0, None),
        ("%.3d", 64, "I1", 7, 0, None),
        ("%.0d", 64, "I1", 0, 0, None),           # precision 0 + value 0 -> empty
        ("%.0d", 64, "I1", 5, 0, None),
        ("%8.3d", 64, "I1", -7, 0, None),
        ("%-8.3d", 64, "I1", -7, 0, None),
        ("%08.3d", 64, "I1", -7, 0, None),        # '0' ignored when precision given
        ("%0d", 64, "I1", 0, 0, None),
        # --- two arguments ---
        ("%d,%d", 64, "I2", 3, 4, None),
        ("[%5d|%-5d]", 64, "I2", -3, 3, None),
        ("(%d,%d)", 64, "I2", -2147483648, 2147483647, None),
        ("%d%%", 64, "I1", 50, 0, None),
        # --- unsigned ---
        ("%u", 64, "U1", 0, 0, None),
        ("%u", 64, "U1", 4294967295, 0, None),
        ("%u", 64, "U1", 4000000000, 0, None),
        ("%10u", 64, "U1", 42, 0, None),
        ("%-10u", 64, "U1", 42, 0, None),
        ("%010u", 64, "U1", 42, 0, None),
        ("%.0u", 64, "U1", 0, 0, None),
        # --- hex ---
        ("%x", 64, "U1", 0, 0, None),
        ("%x", 64, "U1", 3735928559, 0, None),    # 0xDEADBEEF
        ("%X", 64, "U1", 3735928559, 0, None),
        ("%#x", 64, "U1", 0, 0, None),            # '#' is ignored for zero
        ("%#x", 64, "U1", 255, 0, None),
        ("%#X", 64, "U1", 255, 0, None),
        ("%08x", 64, "U1", 255, 0, None),
        ("%#010x", 64, "U1", 255, 0, None),
        ("%-6x", 64, "U1", 255, 0, None),
        ("%.4x", 64, "U1", 255, 0, None),
        ("%#.4x", 64, "U1", 255, 0, None),
        # --- char ---
        ("%c", 64, "C1", ord("A"), 0, None),
        ("%c", 64, "C1", ord(" "), 0, None),
        ("%c", 64, "C1", ord("0"), 0, None),
        ("%5c", 64, "C1", ord("A"), 0, None),
        ("%-5c", 64, "C1", ord("A"), 0, None),
        # --- string ---
        ("%s", 64, "S1", 0, 0, ""),
        ("%s", 64, "S1", 0, 0, "a"),
        ("%s", 64, "S1", 0, 0, "hello"),
        ("%s", 64, "S1", 0, 0, "hello world"),
        ("%10s", 64, "S1", 0, 0, "hi"),
        ("%-10s", 64, "S1", 0, 0, "hi"),
        ("%.3s", 64, "S1", 0, 0, "hello"),
        ("%10.3s", 64, "S1", 0, 0, "hello"),
        ("%-10.3s", 64, "S1", 0, 0, "hello"),
        ("%.0s", 64, "S1", 0, 0, "hello"),
        ("%s", 64, "S1", 0, 0, big),
        # --- literals ---
        ("%%", 64, "NONE", 0, 0, None),
        ("100%%", 64, "NONE", 0, 0, None),
        ("a%%b", 64, "NONE", 0, 0, None),
        ("no specifiers at all", 64, "NONE", 0, 0, None),
        # --- pointer (non-NULL; the header defines %p as 0x + lowercase hex) ---
        ("%p", 64, "P1", 0x1234, 0, None),
        ("%p", 64, "P1", 0xABCDEF, 0, None),
        ("[%p]", 64, "P1", 0x1, 0, None),
        # --- truncation: the same output through small buffers ---
        ("%d", 0, "I1", 12345, 0, None),          # n == 0: nothing written
        ("%d", 1, "I1", 12345, 0, None),          # room for the NUL only
        ("%d", 2, "I1", 12345, 0, None),
        ("%d", 4, "I1", 12345, 0, None),
        ("%s", 3, "S1", 0, 0, "hello"),
        ("%s", 1, "S1", 0, 0, "x"),
        ("%08x", 4, "U1", 255, 0, None),
        ("%.3s", 2, "S1", 0, 0, "hello"),
        ("%d,%d", 3, "I2", 1, 2, None),
        ("%10.3s", 5, "S1", 0, 0, "hello"),
    ]


def random_cases(rng, count=16):
    """Random corner-ish cases: the expected values change with the seed, so a
    hardcoded implementation cannot survive a fresh run."""
    out = []
    for _ in range(count):
        kind = rng.choice(["d", "d", "u", "x", "X", "s", "c", "p"])
        width = rng.choice(["", "", "2", "6", "9"])
        prec = rng.choice(["", "", ".1", ".3", ".0"])
        # Flags are only drawn where the standard gives them meaning: '#' with
        # %d/%u, a precision with %c, flags with %s other than '-', anything but
        # width with %p are all undefined. Testing undefined behaviour against
        # one particular libc would be unfair, not thorough.
        if kind == "d":
            spec = "%" + "".join(f for f in ["-", "0", "+", " "] if rng.random() < 0.25) + width + prec + kind
        elif kind == "u":
            spec = "%" + "".join(f for f in ["-", "0"] if rng.random() < 0.25) + width + prec + kind
        elif kind in ("x", "X"):
            spec = "%" + "".join(f for f in ["-", "0", "#"] if rng.random() < 0.3) + width + prec + kind
        elif kind == "s":
            spec = "%" + ("-" if rng.random() < 0.3 else "") + width + prec + kind
        elif kind == "c":
            spec = "%" + ("-" if rng.random() < 0.3 else "") + width + kind
        else:                                   # %p: width only
            spec = "%" + width + kind
        n = rng.choice([0, 1, 2, 5, 9, 16, 64])
        if kind == "d":
            out.append((spec, n, "I1", rng.choice(
                [0, 1, -1, 42, -42, 2147483647, -2147483648,
                 rng.randint(-10 ** 6, 10 ** 6)]), 0, None))
        elif kind == "u":
            out.append((spec, n, "U1", rng.choice(
                [0, 1, 4294967295, 4294967294, rng.randrange(2 ** 32)]), 0, None))
        elif kind in ("x", "X"):
            out.append((spec, n, "U1", rng.choice(
                [0, 1, 255, 4096, 3735928559, rng.randrange(2 ** 32)]), 0, None))
        elif kind == "s":
            s = "".join(rng.choice("abcXYZ019 _-") for _ in range(rng.randint(0, 14)))
            out.append((spec, n, "S1", 0, 0, s))
        elif kind == "c":
            out.append((spec, n, "C1", rng.choice([ord("A"), ord("z"), ord("0"),
                                                   ord(" "), ord("%")]), 0, None))
        else:
            out.append((spec, n, "P1", rng.choice([1, 0x40, 0x1234, 0xDEADBEEF]), 0, None))
    return out


def c_str(s):
    """C string literal for arbitrary ASCII text: escape what must be escaped.
    Non-printables go out as 3-digit octal escapes, which (unlike \\x) cannot
    run on into the following character."""
    out = []
    for ch in s:
        if ch == '"':
            out.append('\\"')
        elif ch == '\\':
            out.append('\\\\')
        elif ch == '\n':
            out.append('\\n')
        elif ch == '\t':
            out.append('\\t')
        elif ch == '\r':
            out.append('\\r')
        elif 32 <= ord(ch) < 127:
            out.append(ch)
        else:
            out.append('\\%03o' % ord(ch))
    return '"' + "".join(out) + '"'


def emit_vectors(path, cases, stream):
    L = []
    w = L.append
    w("/* generated by tests/gen_printf_vectors.py - do not edit */")
    w("#ifndef PRINTF_VECTORS_H")
    w("#define PRINTF_VECTORS_H")
    w("")
    w("/* Shared conventions - the host reference and the firmware gate app MUST")
    w("   implement these identically:")
    w("     buffer   : PF_BUF bytes, pre-filled with 0xA5 before every call")
    w("     written  : (n > 0 && ret >= 0) ? min(ret+1, n) : 0")
    w("     guard_ok : every byte in [written, PF_BUF) is still 0xA5")
    w("     out[]    : first min(n, PF_OUT) bytes of the buffer */")
    w("#define PF_BUF 64u")
    w("#define PF_OUT 12u")
    w("")
    w("/* argument shapes */")
    for i, s in enumerate(SHAPES):
        w("#define PF_%s %d" % (s, i))
    w("")
    w("typedef struct { const char *fmt; unsigned n; int shape; long a, b;")
    w("                 const char *s; } pf_case_t;")
    w("static const pf_case_t pf_cases[] = {")
    for fmt, n, shape, a, b, s in cases:
        w("  { %s, %uu, PF_%s, %dL, %dL, %s }," % (
            c_str(fmt), n, shape, a, b, c_str(s) if s is not None else "0"))
    w("};")
    w("#define N_PF_CASES %uu" % len(cases))
    w("")
    w("typedef struct { const char *fmt; int shape; long a, b; const char *s; } pf_stream_t;")
    w("static const pf_stream_t pf_stream[] = {")
    for fmt, shape, a, b, s in stream:
        w("  { %s, PF_%s, %dL, %dL, %s }," % (
            c_str(fmt), shape, a, b, c_str(s) if s is not None else "0"))
    w("};")
    w("#define N_PF_STREAM %uu" % len(stream))
    w("")
    w("/* filled in by the host reference run (see tests/test_printf.sh) */")
    w("typedef struct { int ret; unsigned char out[PF_OUT]; unsigned guard_ok; } pf_expect_t;")
    w("")
    w("#endif")
    write(path, "\n".join(L) + "\n")


HOST_REF_C = r"""/* generated by tests/gen_printf_vectors.py --emit-host
 *
 * Runs the F2 vectors through the HOST C library's snprintf, which is the
 * reference for this gate. argv[1] = expectations header, argv[2] = expected
 * bytes of the stream block. Nothing here is firmware code.
 */
#include <stdio.h>
#include <string.h>
#include "printf_vectors.h"

static unsigned char buf[PF_BUF];

static void fill(void)
{
    for (unsigned i = 0; i < PF_BUF; i++) buf[i] = 0xA5;
}

static int call(const pf_case_t *c)
{
    switch (c->shape) {
    case PF_I1:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a);
    case PF_U1:   return snprintf((char *)buf, c->n, c->fmt, (unsigned)c->a);
    case PF_IU:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a, (unsigned)c->b);
    case PF_S1:   return snprintf((char *)buf, c->n, c->fmt, c->s);
    case PF_C1:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a);
    case PF_P1:   return snprintf((char *)buf, c->n, c->fmt, (void *)(unsigned long)c->a);
    case PF_I2:   return snprintf((char *)buf, c->n, c->fmt, (int)c->a, (int)c->b);
    default:      return snprintf((char *)buf, c->n, c->fmt);
    }
}

static unsigned written_of(int ret, unsigned n)
{
    if (n == 0 || ret < 0) return 0;
    return ((unsigned)ret + 1u <= n) ? (unsigned)ret + 1u : n;
}

int main(int argc, char **argv)
{
    if (argc < 3) { fprintf(stderr, "usage: %s <expect.h> <stream.txt>\n", argv[0]); return 2; }

    FILE *h = fopen(argv[1], "w");
    if (!h) { fprintf(stderr, "cannot write %s\n", argv[1]); return 2; }
    fprintf(h, "/* generated by the host C library (tests/printf_ref_host) - do not edit */\n");
    fprintf(h, "#ifndef PRINTF_EXPECTED_H\n#define PRINTF_EXPECTED_H\n");
    fprintf(h, "static const pf_expect_t pf_expect[N_PF_CASES] = {\n");
    for (unsigned i = 0; i < N_PF_CASES; i++) {
        const pf_case_t *c = &pf_cases[i];
        fill();
        int ret = call(c);
        unsigned written = written_of(ret, (unsigned)c->n);
        int guard = 1;
        for (unsigned k = written; k < PF_BUF; k++)
            if (buf[k] != 0xA5) { guard = 0; break; }
        unsigned shown = (c->n < PF_OUT) ? c->n : PF_OUT;
        fprintf(h, "  { %d, { ", ret);
        for (unsigned k = 0; k < PF_OUT; k++)
            fprintf(h, "0x%02x, ", (k < shown) ? buf[k] : 0);
        fprintf(h, "}, %u },\n", (unsigned)guard);
    }
    fprintf(h, "};\n#endif\n");
    fclose(h);

    if (!freopen(argv[2], "w", stdout)) { fprintf(stderr, "cannot write %s\n", argv[2]); return 2; }
    for (unsigned i = 0; i < N_PF_STREAM; i++) {
        const pf_stream_t *c = &pf_stream[i];
        switch (c->shape) {
        case PF_I1:   printf(c->fmt, (int)c->a); break;
        case PF_U1:   printf(c->fmt, (unsigned)c->a); break;
        case PF_IU:   printf(c->fmt, (int)c->a, (unsigned)c->b); break;
        case PF_S1:   printf(c->fmt, c->s); break;
        case PF_C1:   printf(c->fmt, (int)c->a); break;
        case PF_P1:   printf(c->fmt, (void *)(unsigned long)c->a); break;
        case PF_I2:   printf(c->fmt, (int)c->a, (int)c->b); break;
        default:      printf(c->fmt); break;
        }
    }
    fflush(stdout);
    return 0;
}
"""


def write(path, text):
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--out", default="firmware/apps")
    ap.add_argument("--emit-host", metavar="FILE", required=True)
    args = ap.parse_args()

    seed = args.seed if args.seed is not None else random.randrange(1 << 30)
    rng = random.Random(seed)

    cases = fixed_cases() + random_cases(rng)
    emit_vectors(os.path.join(args.out, "printf_vectors.h"), cases, STREAM)
    write(args.emit_host, HOST_REF_C)

    print("SEED=%d" % seed)
    print("N_CASES=%d" % len(cases))
    print("N_STREAM=%d" % len(STREAM))


if __name__ == "__main__":
    main()
