#!/usr/bin/env bash
# check.sh — assertion helpers for the firmware-track gates. Source it:
#
#   . "$(dirname "$0")/check.sh"
#   LOG=$(mktemp); trap 'rm -f "$LOG"' EXIT
#   tests/run_app.sh libc_test > "$LOG" 2>&1
#   check_init "$LOG" "libc"
#   check_has '\[LIB\] [0-9]+/[0-9]+ PASS' 'vector summary'
#   check_absent '\[LIB\].*FAIL' 'no failing vectors'
#   check_pass
#
# Output style follows tests/test_shell.sh (ok / FAIL lines). check_pass
# returns non-zero if anything failed, so a gate can just `exit $?`.
set -u

check_init() {
    CHECK_LOG="${1:?check_init needs the log file}"
    CHECK_NAME="${2:-gate}"
    CHECK_FAILED=0
    CHECK_TOTAL=0
    [ -f "$CHECK_LOG" ] || { echo "FAIL  log not found: $CHECK_LOG"; exit 1; }
}

# check_has <eregex> <label> — pattern must appear at least once
check_has() {
    CHECK_TOTAL=$((CHECK_TOTAL + 1))
    if grep -Eq "$1" "$CHECK_LOG"; then printf 'ok    %s\n' "$2"
    else printf 'FAIL  %s\n' "$2"; CHECK_FAILED=$((CHECK_FAILED + 1)); fi
}

# check_count <eregex> <n> <label> — pattern must appear exactly n times
check_count() {
    CHECK_TOTAL=$((CHECK_TOTAL + 1))
    local n
    n=$(grep -cE "$1" "$CHECK_LOG" || true)
    if [ "$n" -eq "$2" ]; then printf 'ok    %s\n' "$3"
    else
        printf 'FAIL  %s (saw %s, want %s)\n' "$3" "$n" "$2"
        CHECK_FAILED=$((CHECK_FAILED + 1))
    fi
}

# check_absent <eregex> <label> — pattern must NOT appear
check_absent() {
    CHECK_TOTAL=$((CHECK_TOTAL + 1))
    if grep -Eq "$1" "$CHECK_LOG"; then
        printf 'FAIL  %s\n' "$2"
        printf '        offending lines:\n'
        grep -E "$1" "$CHECK_LOG" | head -5 | sed 's/^/        /'
        CHECK_FAILED=$((CHECK_FAILED + 1))
    else
        printf 'ok    %s\n' "$2"
    fi
}

check_pass() {
    if [ "$CHECK_FAILED" -ne 0 ]; then
        printf '%s: FAILED (%s of %s checks)\n' "$CHECK_NAME" "$CHECK_FAILED" "$CHECK_TOTAL"
        printf 'transcript: %s\n' "$CHECK_LOG"
        return 1
    fi
    printf '%s: ALL PASS (%s checks)\n' "$CHECK_NAME" "$CHECK_TOTAL"
    return 0
}
