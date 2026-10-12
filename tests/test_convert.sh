#!/usr/bin/env bash
# F1b gate — numeric conversion (firmware track).
#
#   ./tests/test_convert.sh [--seed N]
#
# Same app as test_libc.sh, built with APP_DEFS=-DWITH_CONV so the strtoul /
# strtol / itoa / utoa_hex families are included. It therefore needs the
# memory+string half to link too, which is why F1a comes first.
#
# The expectations for strtoul/strtol are computed in Python (independent of
# your C) and cross-checked against the host C library by the generated
# --emit-refcheck program; see the generator's docstring.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SEED=""
while [ $# -gt 0 ]; do
    case "$1" in
        --seed) SEED="${2:?--seed needs a value}"; shift 2 ;;
        -h|--help) sed -n '2,10p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "usage: tests/test_convert.sh [--seed N]" >&2; exit 2 ;;
    esac
done

. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

if [ -n "$SEED" ]; then
    out="$(python3 "$ROOT/tests/gen_libc_vectors.py" --seed "$SEED" --out "$ROOT/firmware/apps")"
else
    out="$(python3 "$ROOT/tests/gen_libc_vectors.py" --out "$ROOT/firmware/apps")"
fi
printf '%s\n' "$out"
seed="$(printf '%s\n' "$out" | sed -n 's/^SEED=//p')"
total_mem="$(printf '%s\n' "$out" | sed -n 's/^TOTAL_MEM=//p')"
total_conv="$(printf '%s\n' "$out" | sed -n 's/^TOTAL_CONV=//p')"
if [ -z "$seed" ] || [ -z "$total_mem" ] || [ -z "$total_conv" ]; then
    echo "FAIL  vector generator produced no SEED/TOTAL_* lines" >&2
    exit 1
fi
total=$((total_mem + total_conv))

APP_DEFS="-DWITH_CONV" "$ROOT/tests/run_app.sh" libc_test > "$LOG" 2>&1

check_init "$LOG" "libc conversion (seed=$seed)"

check_has "\[TEST\] libc gate seed=${seed} " 'app was built with this seed'\''s vectors'
check_has "\[TEST\] SUMMARY ${total}/${total}" "summary is ${total}/${total} (mem ${total_mem} + conv ${total_conv})"
check_absent '\[TEST\] [a-z_]+\[[0-9]+\] FAIL' 'no failing vector'
check_has '\[TEST\] DONE' 'app signalled completion'

check_pass
