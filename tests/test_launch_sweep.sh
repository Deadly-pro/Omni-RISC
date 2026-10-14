#!/usr/bin/env bash
# Launch-cost sweep gate — what a GPU launch costs, and per instruction after that.
#
#   ./tests/test_launch_sweep.sh
#
# Kernel length is generated in C (N x ADD + HALT), so N is exact. The app sweeps
# 4..256 instructions and fits two points far apart, giving the fixed per-launch
# cost (intercept) and the marginal cost per kernel instruction (slope). Those two
# numbers are what answers "how big must a kernel be before launching beats doing
# the work on the CPU" - the question the 306-cycle launch latency raised.
#
# Exit 0 = the sweep ran, grew with kernel length, and produced a plausible fit.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
OUT="$(mktemp)"
ERR="$(mktemp)"
trap 'rm -f "$LOG" "$OUT" "$ERR"' EXIT

TB_QUIET=16000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" launch_sweep > "$OUT" 2> "$ERR"
cat "$OUT" "$ERR" > "$LOG"

check_init "$LOG" "launch sweep"

for n in 4 32 40 48 56 64; do
    check_has "\[SWEEP\] n=$n cycles=[0-9]+ per_insn_x100=[0-9]+" "kernel of $n instructions measured"
done
check_has '\[SWEEP\] flaky n=16 attempt1=[0-9]+ attempt2=[0-9]+' \
          'the intermittent no-completion case is measured, not averaged away'
check_has '\[SWEEP\] slope=[0-9]+ cycles_per_kernel_insn intercept=[0-9]+ fixed_launch_cycles' \
          'fixed launch cost and marginal per-instruction cost separated'
check_has '\[TEST\] sweep_monotonic PASS' 'longer kernels take longer'
check_has '\[TEST\] sweep_fixed_cost PASS' 'fixed launcher cost is non-trivial'
check_has '\[TEST\] sweep_marginal PASS' 'marginal cost per kernel instruction is sane'
check_has '\[TEST\] DONE' 'app signalled completion'

echo "--- launch cost, fixed and marginal ---"
grep -a '^\[SWEEP\]' "$LOG" | sed 's/^/  /'

check_pass
