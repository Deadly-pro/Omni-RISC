#!/usr/bin/env bash
# P2 gate — a real workload: a 2-layer MLP on the GPU, kernel image reloaded
# between layers (firmware track).
#
#   ./tests/test_mlp.sh
#
#   firmware/apps/mlp_test.c   the workload + the scalar reference
#
# What makes this more than "one vector add": the GPU's instruction memory is
# reloaded four times (matmul, relu, matmul, relu) while the computation is in
# flight, which is only possible because F8 gave the GPU a load port. The CPU
# stages each layer's operands through the shared-memory window and reads the
# results back, and the app requires the GPU's answer to match the same MLP
# computed scalar in C - per lane, four lanes at once.
#
# Exit 0 = all four lanes matched the scalar reference and the image really was
# reloaded between launches.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
OUT="$(mktemp)"
ERR="$(mktemp)"
trap 'rm -f "$LOG" "$OUT" "$ERR"' EXIT

python3 "$ROOT/tests/gen_gpu_kernel.py" \
    --emit-multi firmware/gpu_kernels/matmul.S,firmware/gpu_kernels/relu.S >/dev/null \
    || { echo "kernel assembly failed" >&2; exit 2; }

# Firmware transcript on stdout, harness diagnostics on stderr - kept apart so a
# heartbeat cannot land inside a firmware line (see tests/test_bench.sh).
TB_QUIET=8000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" mlp_test > "$OUT" 2> "$ERR"
cat "$OUT" "$ERR" > "$LOG"

check_init "$LOG" "mlp"

check_has '\[MLP\] 2-layer MLP, 4 samples in parallel' 'workload started with all four lanes in play'
check_has '\[MLP\] kernel images: matmul=[0-9]+ words, relu=[0-9]+ words' 'two distinct kernel images assembled'

# the whole point: the GPU answer is checked against the scalar computation
check_has '\[TEST\] lane0 PASS' 'lane 0 matched the scalar reference'
check_has '\[TEST\] lane1 PASS' 'lane 1 matched the scalar reference'
check_has '\[TEST\] lane2 PASS' 'lane 2 matched the scalar reference'
check_has '\[TEST\] lane3 PASS' 'lane 3 matched the scalar reference'
check_has '\[TEST\] reload PASS 4 loads' 'the GPU image was reloaded between launches'
check_has '\[TEST\] cost PASS' 'both paths timed'
check_has '\[MLP\] gpu_cycles=[0-9]+ scalar_cycles=[0-9]+ ramp_ratio_x100=[0-9]+' \
          'GPU vs scalar cycle cost reported'
check_has '\[TEST\] SUMMARY 6/6' 'all six families passed'
check_has '\[TEST\] DONE' 'app signalled completion'

echo "--- workload result ---"
grep -a '^\[MLP\] gpu_cycles' "$LOG" | sed 's/^/  /'

check_pass
