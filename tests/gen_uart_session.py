#!/usr/bin/env python3
"""gen_uart_session.py — host sessions for the F3 UART gate.

Writes two files:
  <out>/uart_burst.bin    N bytes: a running 8-bit counter 0,1,2,...,N-1
                          (N < 256 so loss inference stays unambiguous)
  <out>/uart_line.txt     "HELLO12345\\r\\n"

The counter pattern is what makes the burst test an *accounting* test rather
than a "did anything arrive" test: consecutive received bytes differ by (1 +
however many the RTL FIFO dropped), so the firmware can reconstruct the loss
exactly. A plain random pattern could not.

Usage: gen_uart_session.py [--out DIR] [--n 200]
Prints: N=<n> LINE=<len>
"""
import argparse
import os


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="tests/generated")
    ap.add_argument("--n", type=int, default=200)
    args = ap.parse_args()
    if not (0 < args.n < 256):
        raise SystemExit("n must be 1..255 so the modular loss inference is unambiguous")

    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "uart_burst.bin"), "wb") as f:
        f.write(bytes(i & 0xFF for i in range(args.n)))

    line = "HELLO12345\r\n"
    with open(os.path.join(args.out, "uart_line.txt"), "w") as f:
        f.write(line)

    print("N=%d" % args.n)
    print("LINE=%d" % (len(line) - 2))     # the payload without CRLF


if __name__ == "__main__":
    main()
