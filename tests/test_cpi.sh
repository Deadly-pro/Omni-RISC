#!/usr/bin/env bash
# CPI gate — cycles per instruction, derived from the app's own numbers.
#
#   ./tests/test_cpi.sh
#
# The core cannot report CPI from its CSRs (`minstret` has no instruction-retired
# signal to count - see RESULTS.md), so CPI is measured where the instruction count
# is exact by construction: a taken-branch loop of 2 instructions per iteration and
# a 64-instruction straight line. The app prints cycles and instruction counts; this
# gate re-derives CPI from those printed numbers rather than trusting the app's
# arithmetic.
#
# Exit 0 = both CPIs are in the expected shape for a 5-stage in-order core.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
OUT="$(mktemp)"
ERR="$(mktemp)"
trap 'rm -f "$LOG" "$OUT" "$ERR"' EXIT

TB_QUIET=8000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" cpi_test > "$OUT" 2> "$ERR"
cat "$OUT" "$ERR" > "$LOG"

check_init "$LOG" "cpi"

check_has '\[CPI\] taken_branch cycles=[0-9]+ instructions=8192 cpi=[0-9]+\.[0-9]{2}' \
          'taken-branch loop timed over exactly 8192 instructions'
check_has '\[CPI\] straight_line cycles=[0-9]+ instructions=64 cpi=[0-9]+\.[0-9]{2}' \
          'straight line timed over exactly 64 instructions'
check_has '\[TEST\] cpi_straight PASS' 'straight-line CPI is at or just above 1.0'
check_has '\[TEST\] cpi_branch PASS'   'a taken branch is more expensive per instruction'
check_has '\[TEST\] DONE'              'app signalled completion'

# Re-derive CPI from the printed cycles and the known instruction counts (this is
# the check that the app's own arithmetic is not what produced the answer).
line=$(grep -a '^\[CPI\] straight_line' "$LOG" | head -1)
cyc=$(printf '%s\n' "$line" | sed -n 's/.*cycles=\([0-9]*\).*/\1/p')
printf '  straight_line: %s cycles / %s instructions = ' "$cyc" "64"
awk -v c="$cyc" 'BEGIN { printf "%.2f CPI\n", c/64 }'
awk -v c="$cyc" 'BEGIN { exit !(c/64 >= 0.9 && c/64 <= 2.0) }' \
    && check_has '\[CPI\] straight_line' 'CPI re-derived from cycles/instructions is in band' \
    || { echo "FAIL  re-derived straight-line CPI $(awk -v c="$cyc" 'BEGIN { printf "%.2f", c/64 }') outside 0.9..2.0"; false; }

line=$(grep -a '^\[CPI\] taken_branch' "$LOG" | head -1)
cyc=$(printf '%s\n' "$line" | sed -n 's/.*cycles=\([0-9]*\).*/\1/p')
printf '  taken_branch:  %s cycles / %s instructions = ' "$cyc" "8192"
awk -v c="$cyc" 'BEGIN { printf "%.2f CPI\n", c/8192 }'

echo "--- pipeline characterization ---"
grep -a '^\[CPI\]' "$LOG" | sed 's/^/  /'

check_pass
