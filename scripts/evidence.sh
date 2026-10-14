#!/usr/bin/env bash
# Regenerate every number quoted in RESULTS.md.
#
#   ./scripts/evidence.sh          # run everything, print the table
#   ./scripts/evidence.sh --quick  # skip the slow RTL testbenches
#
# This is the substitute for a board. There is no silicon and (on this machine) no
# Vivado, so the only thing that makes the claims in RESULTS.md worth anything is
# that a stranger can re-run them. Every row below is a command someone else can
# type; nothing here is a number we typed in by hand.
#
# Runs everything even when something fails - a single failing gate should not
# hide the state of the other twelve.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

QUICK=0
[ "${1:-}" = "--quick" ] && QUICK=1

pass=0; fail=0; failed=""

row() { printf '  %-16s %s\n' "$1" "$2"; }

gate() {
    if bash "tests/$1.sh" >/dev/null 2>&1; then
        pass=$((pass + 1)); row "$1" "PASS"
    else
        fail=$((fail + 1)); failed="$failed $1"; row "$1" "FAIL"
    fi
}

tb() {
    if ./hardware/sim/run_sim.sh "$1" 2>&1 | grep -aq "SIMULATION PASSED"; then
        pass=$((pass + 1)); row "$1" "PASS"
    else
        fail=$((fail + 1)); failed="$failed $1"; row "$1" "FAIL"
    fi
}

echo "=== environment ==="
printf '  %-16s %s\n' verilator  "$(verilator --version 2>/dev/null | head -1)"
printf '  %-16s %s\n' cross-gcc  "$(riscv64-linux-gnu-gcc --version 2>/dev/null | head -1)"
printf '  %-16s %s\n' python     "$(python3 --version 2>/dev/null)"
printf '  %-16s %s\n' vivado     "$(command -v vivado >/dev/null 2>&1 && echo present || echo 'ABSENT - no area/timing numbers can be produced here')"

echo
echo "=== firmware-track gates (each runs a firmware app on the simulated SoC) ==="
# The console TB is what the gates drive; rebuild it so a stale binary cannot
# quietly pass (RTL changes do not otherwise invalidate it).
rm -f hardware/sim/obj_dir_tb_soc_console/Vsoc_top
for g in test_libc test_convert test_printf test_uart test_framing test_alloc \
         test_fault test_timers test_gpu_loader test_bench test_mlp test_mlp_mem \
         test_gpu_mem test_cpi test_launch_sweep test_shell; do
    gate "$g"
done

echo
if [ "$QUICK" -eq 0 ]; then
    echo "=== RTL testbenches (independent of the firmware layer) ==="
    for t in cpu/tb_alu cpu/tb_cpu_top soc/tb_soc_gpu soc/tb_soc_rtos; do
        tb "$t"
    done
    echo
else
    echo "=== RTL testbenches skipped (--quick) ==="
    echo
fi

echo "=== result ==="
printf '  %d passed, %d failed\n' "$pass" "$fail"
[ -n "$failed" ] && echo "  failed:$failed"
echo
echo "  Everything above is SIMULATION (Verilator, cycle-accurate model of this RTL"
echo "  at a nominal 50 MHz). No bitstream exists and nothing has run on silicon."
echo "  There is no measured Fmax, no power number, and no silicon-vs-simulation"
echo "  comparison. Area/timing numbers can only come from a Vivado run."

[ "$fail" -eq 0 ]
