#!/usr/bin/env bash
# F1a gate — memory + string library (firmware track).
#
#   ./tests/test_libc.sh [--seed N]
#
# Generates a fresh seeded vector table, builds firmware/apps/libc_test.c against
# YOUR firmware/lib/*.c, runs it in the simulation and checks the transcript.
# Exit 0 = every vector passed. The seed is printed, and echoed by the app, so a
# failure is always reproducible with --seed.
#
# Before you write anything the gate still RUNS: the app carries weak, wrong
# stubs for every function it calls, so you get per-case PASS/FAIL lines from
# your first function instead of "undefined reference". The gate fails until
# your strong definitions make every case pass.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SEED=""
while [ $# -gt 0 ]; do
    case "$1" in
        --seed) SEED="${2:?--seed needs a value}"; shift 2 ;;
        -h|--help) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "usage: tests/test_libc.sh [--seed N]" >&2; exit 2 ;;
    esac
done

. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

# 1) vectors
if [ -n "$SEED" ]; then
    out="$(python3 "$ROOT/tests/gen_libc_vectors.py" --seed "$SEED" --out "$ROOT/firmware/apps")"
else
    out="$(python3 "$ROOT/tests/gen_libc_vectors.py" --out "$ROOT/firmware/apps")"
fi
printf '%s\n' "$out"
seed="$(printf '%s\n' "$out" | sed -n 's/^SEED=//p')"
total="$(printf '%s\n' "$out" | sed -n 's/^TOTAL_MEM=//p')"
if [ -z "$seed" ] || [ -z "$total" ]; then
    echo "FAIL  vector generator produced no SEED/TOTAL_MEM" >&2
    exit 1
fi

# 2) run (memory + string half only; the numeric half is tests/test_convert.sh)
"$ROOT/tests/run_app.sh" libc_test > "$LOG" 2>&1

check_init "$LOG" "libc mem/string (seed=$seed)"

# The app echoes the seed it was compiled with: catches a stale generated table
# left over from an earlier run.
check_has "\[TEST\] libc gate seed=${seed} " 'app was built with this seed'\''s vectors'
# Exact count: the SUMMARY is printed from the app's own counters, so a
# short-circuited or skipped family cannot reach this number.
check_has "\[TEST\] SUMMARY ${total}/${total}" "summary is ${total}/${total}"
check_absent '\[TEST\] [a-z_]+\[[0-9]+\] FAIL' 'no failing vector'
check_has '\[TEST\] DONE' 'app signalled completion (harness ended on the marker, not the cap)'

check_pass
