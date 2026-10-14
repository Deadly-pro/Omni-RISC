#!/usr/bin/env bash
# F6 gate — fault diagnostics (firmware track).
#
#   ./tests/test_fault.sh
#
# One run per fault kind, because an exception on this core has no fix-up path:
# the handler dumps the machine state and parks. What the gate asserts for the
# access-fault and breakpoint kinds is deliberately written as "either the trap
# happened and was decoded, or the RTL did not raise it and the app said so" -
# this core has no MMU/PMP, so which of these exist is a property of the RTL, and
# guessing it would be the test lying.
#
# Exit 0 = every kind produced the expected evidence.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"

fail=0
kind_run()   # <kind> <check-name> <built> <expect-pattern> <expect-label> [dump-expected]
{
    local kind="$1" name="$2" built="$3" pat="$4" label="$5" dump="${6:-1}"
    local log
    log="$(mktemp)"
    APP_DEFS="-DFAULT_KIND=$kind" TB_START_MARK='[TEST] READY' TB_QUIET=4000000 \
        "$ROOT/tests/run_app.sh" fault_test > "$log" 2>&1
    check_init "$log" "$name"
    check_has "\\[TEST\\] fault gate kind=$kind" 'app built for this kind'
    check_has "$built" 'handler was reached'
    check_has "$pat" "$label"
    # Only where the RTL really raises the trap: a kind that does not trap says so
    # explicitly, and demanding a dump there would be the test inventing hardware.
    if [ "$dump" -eq 1 ]; then
        check_has '\[FAULT\] ra=0x[0-9a-f]{8} t0=0x[0-9a-f]{8}' 'register dump present'
    fi
    check_pass || fail=1
    rm -f "$log"
}

# 0: illegal instruction - must always be decoded, and named
kind_run 0 "fault: illegal instruction" \
    '\[FAULT\] count=1 mcause=0x00000002 name=illegal instruction' \
    '\[FAULT\] mepc=0x[0-9a-f]{8} mtval=0x[0-9a-f]{8}' \
    'mepc and mtval reported'

# 1: misaligned load - this core has no MMU/PMP, so the only access fault it can
# raise is misalignment; and on this RTL it turns out not to raise even that (the
# load just happens), so no dump is demanded - the app says so explicitly.
kind_run 1 "fault: misaligned load" \
    '\[FAULT\] count=1 mcause=0x00000004 name=load address misaligned|\[TEST\] fault none kind=1' \
    '\[FAULT\] mcause=0x00000004 name=load address misaligned|no trap was taken' \
    'misaligned load decoded (or explicitly absent)' 0

# 2: ecall from M-mode - the deliberate exit
kind_run 2 "fault: ecall exit" \
    '\[FAULT\] count=1 mcause=0x0000000b name=ecall from M-mode' \
    '\[FAULT\] ecall -> semihost exit' \
    'ecall is a semihost exit, not a crash'

# 3: ebreak
kind_run 3 "fault: ebreak" \
    '\[FAULT\] count=1 mcause=0x00000003 name=breakpoint|\[TEST\] fault none kind=3' \
    'name=breakpoint|no trap was taken' \
    'ebreak decoded (or explicitly absent)'

exit $fail
