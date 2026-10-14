#!/usr/bin/env bash
# F7 gate — time and software timers (firmware track).
#
#   ./tests/test_timers.sh
#
# The app checks itself: the rollover-safe difference is unit-tested at the
# 32-bit boundary, delays are measured with mtime rather than assumed, the
# one-shot has to fire exactly once and not again, three same-period timers have
# to fire repeatedly in registration order, and a cancelled timer must never run
# (including one cancelled from inside another callback).
#
# Exit 0 = every family passed.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

# The app spins on mtime while waiting for timers, so the harness's "stopped
# talking" window has to be generous.
TB_QUIET=8000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" tick_test > "$LOG" 2>&1

check_init "$LOG" "timers"

check_has '\[TEST\] tick gate tick_us=1000 timers=8' 'app built with the documented tick and table size'
check_has '\[TEST\] elapsed PASS'  'rollover-safe difference correct at the 32-bit boundary'
check_has '\[TEST\] mono PASS'     'ticks_us advances by at least the requested delay'
check_has '\[TEST\] delay PASS'    'delay_ms(10) measured within 25%'
check_has '\[TEST\] oneshot PASS'  'one-shot fired exactly once, promptly'
check_has '\[TEST\] oneshot-once PASS' 'and did not repeat'
check_has '\[TEST\] periodic PASS' 'three same-period timers fired repeatedly, in registration order'
check_has '\[TEST\] cancel PASS'   'a cancelled timer never fired'
check_has '\[TEST\] cancel-from-cb PASS' 'cancel from inside a callback took effect'
check_has '\[TEST\] tick fires=[0-9]+ late=[0-9]+ max_late_us=[0-9]+' 'service statistics reported'
check_absent '\[TEST\] [a-z-]+ FAIL' 'no family failed'
check_has '\[TEST\] SUMMARY 8/8'   'all eight families passed'
check_has '\[TEST\] DONE'          'app signalled completion'

check_pass
