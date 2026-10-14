#!/usr/bin/env bash
# F5 gate — heap allocator (firmware track).
#
#   ./tests/test_alloc.sh [--seed N]
#
# No host reference is needed here: the properties are self-evident (alignment,
# no overlap after writing, coalescing, leaks) and the allocator ships its own
# invariant checker, which the app runs after *every* operation in the soak. A
# failure names the operation number, so the first bad op is the bug.
#
# Exit 0 = every family passed, the arena is coherent, and nothing leaked.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SEED=""
while [ $# -gt 0 ]; do
    case "$1" in
        --seed) SEED="${2:?}"; shift 2 ;;
        -h|--help) sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "usage: tests/test_alloc.sh [--seed N]" >&2; exit 2 ;;
    esac
done

. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

APP_DEFS="${SEED:+-DALLOC_SEED=${SEED}u}" \
TB_START_MARK='[TEST] READY' TB_PACE=line TB_QUIET=8000000 \
    "$ROOT/tests/run_app.sh" alloc_test > "$LOG" 2>&1

check_init "$LOG" "allocator${SEED:+ (seed=$SEED)}"

check_has '\[TEST\] alloc gate arena=4096 align=8' 'app built with the documented arena and alignment'
check_has '\[TEST\] align PASS'        'every allocation is 8-byte aligned'
check_has '\[TEST\] write PASS'        'eight blocks written and read back with no overlap'
check_has '\[TEST\] coalesce PASS'     'freeing neighbours merged them back into one block'
check_has '\[TEST\] realloc PASS'      'realloc preserved contents through grow and shrink'
check_has '\[TEST\] exhausted PASS'    'arena came back completely after freeing everything'
check_has '\[TEST\] doublefree PASS'   'a double free was caught, not corrupted'
check_has '\[TEST\] soak PASS'         '2000 randomised ops with invariants checked after each'
# The double-free family asserts the exact count itself; by the end of the run
# heap_init() has cleared the counter several times, so only the line's presence
# is meaningful here.
check_has '\[TEST\] alloc .* errors=' 'final statistics reported'
check_absent '\[TEST\] [a-z]+ FAIL'    'no family failed'
check_has '\[TEST\] SUMMARY 7/7'       'all seven families passed'
check_has '\[TEST\] DONE'              'app signalled completion'

check_pass
