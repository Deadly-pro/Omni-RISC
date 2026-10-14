#!/usr/bin/env bash
# F3 gate — UART software receive path (firmware track).
#
#   ./tests/test_uart.sh [--seed N]     (accepted for uniformity; deterministic)
#
# Two runs, because the two things worth proving need opposite host pacing:
#
#   1. burst  — the host fires a running counter pattern with no pacing. The
#      16-byte RTL FIFO overruns on purpose. The firmware must report the loss it
#      can reconstruct, not pretend it received everything: consumed + inferred
#      loss must equal the number of bytes the host sent, and the sticky overrun
#      must have been seen.
#   2. line   — the host paces one line at a time, so nothing may be lost:
#      uart_rx_getline returns the exact text with zero overruns, and a following
#      call with a tick timeout gives up on time.
#
# Exit 0 = both runs pass.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SEED=""
while [ $# -gt 0 ]; do
    case "$1" in
        --seed) SEED="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,18p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "usage: tests/test_uart.sh [--seed N]" >&2; exit 2 ;;
    esac
done

. "$ROOT/tests/check.sh"
GEN="$ROOT/tests/generated"
mkdir -p "$GEN"

gen="$(python3 "$ROOT/tests/gen_uart_session.py" --out "$GEN" --n 200)"
printf '%s\n' "$gen"
N="$(printf '%s\n' "$gen" | sed -n 's/^N=//p')"
[ -n "$N" ] || { echo "FAIL  session generator produced no N" >&2; exit 1; }

fail=0

# ---------------------------------------------------------------- burst -----
LOG="$(mktemp)"
# The app deliberately stalls - that is what makes the RTL FIFO overrun - so the
# harness's "the firmware has stopped talking" window must be longer than the
# stall, or the run is declared over before the verdict is printed.
TB_QUIET=8000000 TB_PACE=burst TB_START_MARK='[TEST] READY' \
APP_DEFS="-DUART_RX_BURST -DUART_RX_BURST_N=$N" \
    "$ROOT/tests/run_app.sh" uart_rx_test "$GEN/uart_burst.bin" > "$LOG" 2>&1

check_init "$LOG" "uart burst (N=$N)"
check_has "\[TEST\] uart gate mode=burst n=$N" 'app built in burst mode'
check_has "\[TEST\] burst PASS consumed=[0-9]+ lost=[0-9]+ total=$N overruns=[1-9]" \
          "accounting closes: consumed + reconstructed loss == $N, overrun reported"
check_has '\[TEST\] SUMMARY 1/1' 'the burst family passed'
check_has '\[TEST\] DONE' 'app signalled completion'
check_pass || fail=1
rm -f "$LOG"

# ----------------------------------------------------------------- line -----
LOG="$(mktemp)"
TB_PACE=line TB_START_MARK='[TEST] READY' \
APP_DEFS="-DUART_RX_LINE" \
    "$ROOT/tests/run_app.sh" uart_rx_test "$GEN/uart_line.txt" > "$LOG" 2>&1

check_init "$LOG" "uart line + timeout"
check_has '\[TEST\] uart gate mode=line' 'app built in line mode'
check_has '\[TEST\] line PASS len=10 text=HELLO12345 overruns=0' \
          'paced line arrives intact, in order, with no overrun'
check_has '\[TEST\] timeout PASS ret=-1 elapsed=[0-9]+ asked=2000000' \
          'getline gave up on time instead of hanging'
check_has '\[TEST\] SUMMARY 2/2' 'both line-mode families passed'
check_has '\[TEST\] DONE' 'app signalled completion'
check_pass || fail=1
rm -f "$LOG"

exit $fail
