#!/usr/bin/env bash
# Characterization gate (no board, no Vivado on this host).
#
#   ./tests/test_bench.sh
#
# Runs firmware/apps/bench_test.c on the SoC and prints the table it produces:
# cycles, retired instructions, CPI and cycles-per-unit for each CPU workload,
# plus the GPU-vs-scalar comparison, all read from the core's own mcycle/minstret
# CSRs (0xB00/0xB02) in the Verilator model.
#
# These are SIMULATED cycles. There is no silicon and the clock frequency is not
# measured, so nothing here should be quoted as hardware performance.
#
# The gate's own assertions are about the harness being trustworthy, not about
# specific cycle counts: every workload must scale roughly 2x from N to 2N (a
# benchmark that does not scale with its input is measuring the wrong thing), and
# the timed GPU run must still compute the right answer.
#
# Exit 0 = the characterization ran and behaved.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
OUT="$(mktemp)"
ERR="$(mktemp)"
trap 'rm -f "$LOG" "$OUT" "$ERR"' EXIT

python3 "$ROOT/tests/gen_gpu_kernel.py" >/dev/null || { echo "kernel assembly failed" >&2; exit 2; }

# Capture the two streams separately. The firmware's transcript is stdout and the
# harness's diagnostics (heartbeats, "[TB] done at ... ") are stderr; merging them
# with 2>&1 lets a heartbeat land in the middle of a firmware line and silently
# break every pattern that spans it. Concatenating whole lines afterwards keeps
# both readable and the transcript intact.
TB_QUIET=8000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" bench_test > "$OUT" 2> "$ERR"
cat "$OUT" "$ERR" > "$LOG"

check_init "$LOG" "characterization"

check_has '\[BENCH\] apu characterization, simulated cycles' 'labelled as simulated cycles, not silicon'

# every workload reported, and grew with its input
for w in memcpy_byte memset_byte fnv1a_byte isort_elem matmul8_mac snprintf_line malloc_pair; do
    check_has "\[BENCH\] $w units=[0-9]+ cycles=[0-9]+ mtime=[0-9]+ per_unit=[0-9]+\.[0-9]{2} cksum=[0-9]+" \
              "$w reported cycles, mtime and cycles-per-unit"
    check_has "\[BENCH\] ratio name=$w n_cycles=[0-9]+ n2_cycles=[0-9]+ x100=[0-9]+ skew_pct=[0-9]+ perunit_x100=[0-9]+" \
              "$w measured at N and 2N with the counters cross-checked"
    check_has "\[TEST\] $w PASS" "$w cost more cycles for more input"
done

# the four whose cost is genuinely linear must also cost the same per unit
for w in memcpy_byte memset_byte fnv1a_byte snprintf_line; do
    check_has "\[TEST\] per_unit_$w PASS" "$w per-unit cost stable within 25% at 2N"
done

# the two independent counters have to agree, or the numbers mean nothing
check_has '\[TEST\] counter_agree PASS' 'mcycle and the CLINT mtime agree within 5%'

# the GPU side, and the answer it computed while being timed
check_has '\[BENCH\] gpu_dispatch units=1 cycles=[0-9]+' 'GPU launch->interrupt latency measured'
check_has '\[BENCH\] speedup gpu_launch_cycles=[0-9]+ scalar_cycles=[0-9]+ ratio_x100=[0-9]+ cksum=132' \
          'GPU vs scalar compared, and the kernel computed A+B while timed'
check_has '\[TEST\] gpu_add PASS' 'timed GPU run computed the right answer'
check_has '\[TEST\] SUMMARY 13/13' 'all thirteen families (7 workloads + 4 per-unit + counter agreement + the timed GPU run) passed'
check_has '\[TEST\] DONE' 'app signalled completion'

echo "--- characterization table (simulated cycles) ---"
grep -a '^\[BENCH\]' "$LOG" | sed 's/^/  /'

check_pass
