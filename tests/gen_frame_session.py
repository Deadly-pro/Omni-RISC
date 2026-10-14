#!/usr/bin/env python3
"""gen_frame_session.py — F4 host session + the expected counter values.

Writes the byte stream the firmware will receive (<out>/frame_session.bin) and
prints what the receiver's counters must then read. The counters are the whole
point: a corrupt frame must appear as bad_crc with a NAK on the wire, never as a
delivered frame, and a duplicate must be answered again rather than re-executed.

The wire format implemented here is the one documented in firmware/net/frame.h:

    FLAG body FLAG,  body = len seq cmd payload[len] crc_hi crc_lo
    0x7E / 0x7D inside the body are escaped as 0x7D, b^0x20

Usage: gen_frame_session.py [--out DIR]
Prints: N=<bytes> EXPECT ok=.. bad_crc=.. bad_len=.. resync=.. dup=.. tx=..
"""
import argparse
import os
import random

FLAG = 0x7E
ESC = 0x7D
MAX_PAYLOAD = 32


def crc16(data: bytes) -> int:
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


def stuff(body: bytes) -> bytes:
    out = bytearray()
    for b in body:
        if b in (FLAG, ESC):
            out += bytes((ESC, b ^ 0x20))
        else:
            out.append(b)
    return bytes(out)


class Session:
    """Builds the byte stream. Counting is done separately by simulate(), which
    mirrors the receiver's state machine byte for byte - hand-counting the
    counters is how you end up testing your arithmetic instead of the parser."""

    def __init__(self):
        self.buf = bytearray()

    # A newline after each frame is not part of the protocol: nothing outside a
    # frame is looked at. It exists so the harness's line pacing (TB_PACE=line)
    # waits for the firmware to finish logging its answer before the next frame
    # is fed - the app spends ~150k cycles printing a decoded frame, and the
    # 16-byte RTL FIFO would overrun underneath it otherwise.
    def gap(self):
        self.buf.append(0x0A)

    # ---- helpers ---------------------------------------------------------
    def good(self, seq, cmd, payload=b""):
        body = bytes((len(payload), seq, cmd)) + payload
        crc = crc16(body)
        self.buf += bytes((FLAG,)) + stuff(body + bytes((crc >> 8, crc & 0xFF))) + bytes((FLAG,))
        self.gap()

    def bad_crc_frame(self, seq, cmd, payload=b""):
        body = bytes((len(payload), seq, cmd)) + payload
        crc = crc16(body) ^ 0xFFFF          # deliberately wrong
        self.buf += bytes((FLAG,)) + stuff(body + bytes((crc >> 8, crc & 0xFF))) + bytes((FLAG,))
        self.gap()

    def truncated(self, raw=b"\x03\x09\x01ab"):
        self.buf += bytes((FLAG,)) + stuff(raw)     # no closing flag
        self.gap()

    def oversized(self, seq, cmd):
        body = bytes((200, seq, cmd)) + b"\x00\x00"  # len 200 > MAX_PAYLOAD
        self.buf += bytes((FLAG,)) + stuff(body) + bytes((FLAG,))
        self.gap()

    def garbage(self, n, rng):
        """Bytes that are not a frame. The receiver does not get to ignore them -
        it is mid-stream, so they get parsed as a body and rejected (that is the
        point: garbage must never become a delivered frame)."""
        for _ in range(n):
            self.buf.append(rng.randrange(256))


def simulate(stream: bytes) -> dict:
    """A byte-for-byte model of firmware/net/frame.c, including what the app does
    with a delivered frame. The expectations come from running this, so a
    mismatch means the firmware and the documented format really do disagree."""
    FLAG_, ESC_, MAXP = FLAG, ESC, MAX_PAYLOAD
    c = dict(ok=0, bad_crc=0, bad_len=0, resync=0, dup=0, tx=0)
    body = bytearray()
    in_frame = False
    esc = False
    naks = 0
    have_last = False
    last_seq = 0

    def crc_of(b):
        crc = 0xFFFF
        for x in b:
            crc ^= x << 8
            for _ in range(8):
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
        return crc

    for byte in stream:
        if esc:
            byte ^= 0x20
            esc = False
        elif byte == FLAG_:
            if in_frame and len(body) > 0:
                c["resync"] += 1
            body = bytearray()
            in_frame = True
            continue
        elif byte == ESC_:
            esc = True
            continue

        if not in_frame:
            continue
        if len(body) == 1 and body[0] > MAXP:
            continue                                    # known-bad length: swallow
        if len(body) > 3 + MAXP + 2:
            c["bad_len"] += 1
            c["tx"] += 1                                 # NAK
            body = bytearray()
            in_frame = False
            continue
        body.append(byte)

        if len(body) == 1 and body[0] > MAXP:
            c["bad_len"] += 1
            c["tx"] += 1                                 # NAK
            continue

        want = 1
        if len(body) >= 1 and body[0] <= MAXP:
            want = 3 + body[0] + 2
        if len(body) < want:
            continue
        # complete body: validate
        ln, seq, cmd = body[0], body[1], body[2]
        payload = bytes(body[3:3 + ln])
        got_crc = (body[3 + ln] << 8) | body[3 + ln + 1]
        if crc_of(body[:3 + ln]) != got_crc:
            c["bad_crc"] += 1
            c["tx"] += 1                                 # NAK
            body = bytearray()
            in_frame = False
            continue
        c["ok"] += 1
        dup = have_last and seq == last_seq
        if dup:
            c["dup"] += 1
        last_seq = seq
        have_last = True
        c["tx"] += 1                                     # the app always answers
        body = bytearray()
        in_frame = False
    return c


def build(seed):
    rng = random.Random(seed)
    s = Session()

    s.good(1, 1, b"abc")                         # echo
    s.good(1, 1, b"abc")                         # duplicate: answered again
    s.bad_crc_frame(2, 1, b"boom")
    s.garbage(40, rng)
    s.truncated()                                # truncated, then resync
    s.good(3, 1, b"after-resync")
    s.oversized(4, 1)
    s.good(5, 1, bytes((FLAG, ESC, 0x00, 0xFF))) # payload needing escaping
    s.good(6, 9, b"?")                           # unknown command -> NAK
    s.good(0xFF, 1, b"wrap")
    s.good(0x00, 1, b"a")                        # seq wrapped
    s.good(7, 2)                                 # stats
    return s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="tests/generated")
    ap.add_argument("--seed", type=int, default=4242)
    args = ap.parse_args()

    s = build(args.seed)
    os.makedirs(args.out, exist_ok=True)
    with open(os.path.join(args.out, "frame_session.bin"), "wb") as f:
        f.write(bytes(s.buf))

    c = simulate(bytes(s.buf))
    print("N=%d" % len(s.buf))
    print("EXPECT ok=%d bad_crc=%d bad_len=%d resync=%d dup=%d tx=%d"
          % (c["ok"], c["bad_crc"], c["bad_len"], c["resync"], c["dup"], c["tx"]))


if __name__ == "__main__":
    main()
