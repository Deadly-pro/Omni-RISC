#!/usr/bin/env python3
"""Package the interactive-shell demo: transcript -> .cast + animated GIF.

Input is the RAW decoded-TX stream captured from tb_soc_shell (its stdout,
typed-input echo included, exactly what the UART carried). Run:

    ./hardware/sim/run_sim.sh soc/tb_soc_shell < tests/demo_session.txt \
        > /tmp/shell_tx.txt 2>/dev/null
    python3 scripts/make_shell_demo.py /tmp/shell_tx.txt

Outputs (both committed):
    docs/shell_session.cast  -- asciinema v2 recording, replayable with
                                `asciinema play docs/shell_session.cast`
    docs/shell_demo.gif      -- terminal-style animation for the README

Every byte of content comes from the simulation; only the presentation
timing (typing speed, response delays) is synthesized here.
"""
import json
import sys
import time
import os
from PIL import Image, ImageDraw, ImageFont

TYPE_MS = 85          # per-keystroke delay
ENTER_MS = 140        # pause on Enter before response starts
LINE_MS = 55          # between response lines
BOOT_MS = 400         # initial banner hold
CMD_GAP_MS = 650      # thinking pause between commands

COLS, ROWS = 104, 26
CELL_W, CELL_H = 10, 19
BAR_H = 38
PAD = 14
WIDTH = PAD * 2 + COLS * CELL_W
HEIGHT = BAR_H + PAD + ROWS * CELL_H + PAD

BG      = (13, 15, 20)
BG_BAR  = (25, 29, 38)
FG      = (214, 224, 240)
FG_DIM  = (110, 122, 145)
GREEN   = (126, 231, 135)
ACCENT  = (97, 175, 239)

FONT_PATH = os.path.expanduser(
    "~/.local/share/fonts/JetBrainsMonoNerd/JetBrainsMonoNerdFontMono-Regular.ttf")


def load_events(tx_path):
    """Parse the raw TX stream into a timed (t_ms, kind, text) event list."""
    raw = open(tx_path, "r", errors="replace").read()
    # drop the TB harness chatter; keep only what the UART really carried
    lines = [ln for ln in raw.replace("\r\n", "\n").split("\n")
             if not ln.startswith("[SHELL-TB]")]
    text = "\n".join(lines).strip("\n")
    if not os.path.exists(FONT_PATH):
        sys.exit(f"font not found: {FONT_PATH}")

    cmds = ["help", "uptime", "ps", "ticks", "gpu", "quit"]
    segments = text.split("omni> ")          # seg0 = boot banner
    if len(segments) != len(cmds) + 1:
        sys.exit(f"unexpected transcript: {len(segments)-1} prompts")

    ev = []                                   # (t_ms, 'o'|'i', text)
    t = BOOT_MS

    def out(chunk):
        nonlocal t
        ev.append((t, "o", chunk))

    out(segments[0] + "\n")                   # boot banner incl. ready
    out("omni> ")
    for cmd, seg in zip(cmds, segments[1:]):
        resp = seg[len(cmd):] if seg.startswith(cmd) else seg
        for ch in cmd:                        # typing animation
            ev.append((t, "i", ch))
            out(ch)
            t += TYPE_MS
        t += TYPE_MS
        ev.append((t, "i", "\n"))
        out("\n")
        t += ENTER_MS
        for ln in resp.strip("\n").split("\n"):
            out(ln + "\n")
            t += LINE_MS
        t += CMD_GAP_MS - LINE_MS
        if cmd != "quit":
            out("omni> ")
        else:
            break                             # quit ends the session
    return ev


def write_cast(ev, path):
    meta = {
        "version": 2, "width": COLS, "height": ROWS,
        "timestamp": int(time.time()),
        "env": {"SHELL": "/bin/sh", "TERM": "xterm-256color"},
        "title": "Omni-RISC APU - FreeRTOS shell in Verilator (UART @115200)",
    }
    with open(path, "w") as f:
        f.write(json.dumps(meta) + "\n")
        for t, kind, txt in ev:
            f.write(json.dumps([round(t / 1000, 3), kind, txt]) + "\n")


def render_screen(screen, font, cursor_col, cursor_row, show_cursor):
    img = Image.new("RGB", (WIDTH, HEIGHT), BG)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 0, WIDTH, BAR_H], fill=BG_BAR)
    for i in (6, 12, 18):                     # fake traffic lights
        d.ellipse([i, BAR_H / 2 - 5, i + 10, BAR_H / 2 + 5],
                  fill=(80, 84, 95))
    d.text((34, BAR_H / 2), "omni-risc -- FreeRTOS shell @ 115200 baud "
           "(Verilator, RV32IM+SIMT GPU)", font=font, anchor="lm",
           fill=FG_DIM)
    x0, y0 = PAD, BAR_H + PAD
    for r, row in enumerate(screen):
        color = GREEN if r == cursor_row else FG
        d.text((x0, y0 + r * CELL_H), "".join(row), font=font, fill=color)
    if show_cursor:
        cx = x0 + cursor_col * CELL_W
        cy = y0 + cursor_row * CELL_H
        d.rectangle([cx, cy + 2, cx + CELL_W - 2, cy + CELL_H - 2], fill=FG)
    return img


def write_gif(ev, path):
    font = ImageFont.truetype(FONT_PATH, 15)
    screen = [[" "] * COLS for _ in range(ROWS)]
    row, col = 0, 0

    def put(ch):
        nonlocal row, col
        if ch == "\n":
            row, col = min(row + 1, ROWS - 1), 0
        else:
            if col >= COLS:
                row, col = min(row + 1, ROWS - 1), 0
            screen[row][col] = ch
            col += 1

    frames, delays = [], []
    cur_t, prev_t = 0, None
    pending = ""

    def flush_frame(delay_ms):
        nonlocal pending
        if pending == "" and frames:
            delays[-1] += delay_ms            # extend hold instead of dup
            return
        for ch in pending:
            put(ch)
        pending = ""
        frames.append(render_screen(screen, font, col, row, True))
        delays.append(max(40, min(delay_ms, 2000)))

    for t, kind, txt in ev:
        dt = t - cur_t
        if txt.startswith("omni>"):
            flush_frame(dt)
            for ch in txt:
                put(ch)
            frames.append(render_screen(screen, font, col, row, True))
            delays.append(60)
        elif kind == "o":
            pending += txt
            flush_frame(dt if prev_t is not None else dt)
        else:                                  # keystroke: char already in txt
            pass
        cur_t = t
        prev_t = t
    flush_frame(CMD_GAP_MS)

    pal_frames = [f.quantize(colors=64, method=Image.MEDIANCUT)
                  for f in frames]
    pal_frames[0].save(path, save_all=True, append_images=pal_frames[1:],
                       duration=delays, loop=0, optimize=True)


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ev = load_events(sys.argv[1])
    cast = os.path.join(root, "docs", "shell_session.cast")
    gif = os.path.join(root, "docs", "shell_demo.gif")
    write_cast(ev, cast)
    write_gif(ev, gif)
    print(f"wrote {cast} ({len(ev)} events)")
    print(f"wrote {gif} ({os.path.getsize(gif)//1024} KB)")


if __name__ == "__main__":
    main()
