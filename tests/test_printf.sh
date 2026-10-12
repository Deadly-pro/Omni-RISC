#!/usr/bin/env bash
# F2 gate — printf / snprintf (firmware track).
#
#   ./tests/test_printf.sh [--seed N]
#
# Pipeline:
#   1. tests/gen_printf_vectors.py draws the case table — hand-written corner
#      cases plus a seeded random batch — into firmware/apps/printf_vectors.h,
#      and emits a host program that runs the SAME cases.
#   2. that host program runs them through the HOST C LIBRARY's snprintf and
#      writes firmware/apps/printf_expected.h plus the expected bytes of the
#      stream block. The C library is the reference here, not a reading of the
#      standard: if glibc disagrees with the header contract on a case, that
#      case gets fixed, not worked around.
#   3. the firmware gate app (firmware/apps/printf_test.c) is built against those
#      expectations and run in the simulation; the printf() and vprintf() stream
#      blocks are then diffed byte-for-byte against the host's.
#
# Exit 0 = every case matches the C library and both stream blocks are identical.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

SEED=""
while [ $# -gt 0 ]; do
    case "$1" in
        --seed) SEED="${2:?--seed needs a value}"; shift 2 ;;
        -h|--help) sed -n '2,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "usage: tests/test_printf.sh [--seed N]" >&2; exit 2 ;;
    esac
done

. "$ROOT/tests/check.sh"
GEN="$ROOT/tests/generated"
mkdir -p "$GEN"
LOG="$(mktemp)"
trap 'rm -f "$LOG"' EXIT

# 1) vectors + host reference source
if [ -n "$SEED" ]; then
    out="$(python3 "$ROOT/tests/gen_printf_vectors.py" --seed "$SEED" \
             --out "$ROOT/firmware/apps" --emit-host "$GEN/printf_ref_host.c")"
else
    out="$(python3 "$ROOT/tests/gen_printf_vectors.py" \
             --out "$ROOT/firmware/apps" --emit-host "$GEN/printf_ref_host.c")"
fi
printf '%s\n' "$out"
seed="$(printf '%s\n' "$out" | sed -n 's/^SEED=//p')"
total="$(printf '%s\n' "$out" | sed -n 's/^N_CASES=//p')"
if [ -z "$seed" ] || [ -z "$total" ]; then
    echo "FAIL  vector generator produced no SEED/N_CASES" >&2
    exit 1
fi

# 2) the host C library is the oracle
HOSTCC="${HOSTCC:-cc}"
if ! "$HOSTCC" -O0 -I "$ROOT/firmware/apps" -o "$GEN/printf_ref_host" "$GEN/printf_ref_host.c"; then
    echo "FAIL  host reference would not build" >&2
    exit 1
fi
if ! "$GEN/printf_ref_host" "$ROOT/firmware/apps/printf_expected.h" \
                            "$GEN/printf_stream_expected.txt"; then
    echo "FAIL  host reference run failed" >&2
    exit 1
fi

# 3) firmware gate app
"$ROOT/tests/run_app.sh" printf_test > "$LOG" 2>&1

# 4) assertions on the app's own transcript
check_init "$LOG" "printf (seed=$seed)"
check_has "\[TEST\] printf gate cases=${total} " 'app was built with these vectors'
check_has "\[TEST\] SUMMARY ${total}/${total}" "summary is ${total}/${total}"
check_absent '\[TEST\] snprintf\[[0-9]+\] FAIL' 'no failing snprintf case'
check_has '\[TEST\] DONE' 'app signalled completion'

# 5) both streaming entry points, byte-for-byte against the C library
extract()   # <begin-marker> <end-marker>  -> the lines in between
            # Prefix match, NOT a regex: as a regex, "[PRINTF..." is a character
            # class and awk dies with "invalid range endpoint". Prefix matching
            # also stops the vprintf markers from matching the printf ones.
{
    awk -v b="$1" -v e="$2" 'substr($0,1,length(b)) == b { f=1; next } substr($0,1,length(e)) == e { f=0 } f' "$LOG"
}

stream_check()   # <label> <begin> <end>
{
    local label="$1" got="$GEN/stream_got_$1.txt"
    extract "$2" "$3" > "$got"
    CHECK_TOTAL=$((CHECK_TOTAL + 1))
    if diff -q "$GEN/printf_stream_expected.txt" "$got" >/dev/null 2>&1; then
        printf 'ok    %s stream block matches the host C library\n' "$label"
    else
        printf 'FAIL  %s stream block differs from the host C library\n' "$label"
        diff "$GEN/printf_stream_expected.txt" "$got" | head -12 | sed 's/^/        /'
        CHECK_FAILED=$((CHECK_FAILED + 1))
    fi
}

stream_check printf  '[PRINTF-STREAM-BEGIN]'  '[PRINTF-STREAM-END]'
stream_check vprintf '[VPRINTF-STREAM-BEGIN]' '[VPRINTF-STREAM-END]'

check_pass
