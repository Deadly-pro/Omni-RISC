#!/usr/bin/env bash
# F4 gate — framed host protocol (firmware track).
#
#   ./tests/test_framing.sh [--seed N]
#
# The host side here is a *session generator*, not a live peer: the firmware is
# the receiver, so a deterministic byte stream plus the counter values a correct
# receiver must end up with is a stronger check than a socket dance, and it does
# not depend on pipe timing. tests/gen_frame_session.py builds both, from the
# same model of the wire format.
#
# What the session contains: a good frame, a duplicate of it, a frame with a
# deliberately wrong CRC, garbage that is not a frame, a truncated frame followed
# by a resync, an over-long frame, a payload requiring escaping, an unknown
# command, and a seq that wraps 0xFF -> 0x00.
#
# Exit 0 = the counters match exactly and every reply appeared on the wire.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SEED=4242
while [ $# -gt 0 ]; do
    case "$1" in
        --seed) SEED="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "usage: tests/test_framing.sh [--seed N]" >&2; exit 2 ;;
    esac
done

. "$ROOT/tests/check.sh"
GEN="$ROOT/tests/generated"
mkdir -p "$GEN"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

out="$(python3 "$ROOT/tests/gen_frame_session.py" --out "$GEN" --seed "$SEED")"
printf '%s\n' "$out"
n="$(printf '%s\n' "$out" | sed -n 's/^N=//p')"
expect="$(printf '%s\n' "$out" | sed -n 's/^EXPECT //p')"
[ -n "$n" ] && [ -n "$expect" ] || { echo "FAIL  session generator produced no N/EXPECT" >&2; exit 1; }

# The app idles out after 40 ms of quiet, which is longer than the harness's
# default "stopped talking" window, so give it room.
TB_QUIET=8000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" frame_test "$GEN/frame_session.bin" > "$LOG" 2>&1

check_init "$LOG" "framing (seed=$SEED)"

check_has "\[TEST\] frames ${expect}" 'counters match the reference model exactly'
check_has '\[FRAME\] rx ok seq=1 cmd=1 len=3 hex=616263' 'good frame delivered with its payload'
check_has '\[FRAME\] rx dup seq=1 cmd=1 len=3 hex=616263' 'duplicate detected, not re-executed'
check_has '\[FRAME\] tx echo seq=1' 'the answer went out'
check_has '\[FRAME\] rx ok seq=5 cmd=1 len=4 hex=7e7d00ff' 'escaped payload unescaped byte-for-byte'
check_has '\[FRAME\] rx ok seq=0 cmd=1 len=1 hex=61' 'seq wrapped 0xFF -> 0x00 and still accepted'
check_has '\[FRAME\] tx nak seq=6 \(unknown cmd=9\)' 'unknown command answered with a NAK'
# NB: anchor on 'seq=2 cmd=1' - a bare 'seq=2' also matches 'seq=255', which is a
# different (valid) frame in this session.
check_absent '\[FRAME\] rx ok seq=2 cmd=1' 'the corrupt frame was never delivered'
check_has '\[TEST\] SUMMARY 1/1' 'the family passed'
check_has '\[TEST\] DONE' 'app signalled completion'

check_pass
