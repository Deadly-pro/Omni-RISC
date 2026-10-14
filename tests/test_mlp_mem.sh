#!/usr/bin/env bash
# M3 gate — the MLP computed over shared memory, and what that cost.
#
#   ./tests/test_mlp_mem.sh
#
# STATUS: WORK IN PROGRESS - this gate currently FAILS. Do not add it to CI until
# it passes. What it establishes today is a real measurement, not a success:
#
#   * the computation is WRONG on all four samples (so the MLP's port to LDG/STG
#     is not correct yet, even though tests/test_gpu_mem.sh proves the mechanism
#     works for a simple kernel), and
#   * it costs MORE, not less: 15125 cycles against the MMIO staging window's 6278
#     for the same four samples.
#
# The likely reasons, both worth fixing before believing unified memory is a win:
# every kernel memory access is a bus transaction that must arbitrate against a
# CPU that is polling memory in its wait loop (mem_arbiter gives the CPU priority,
# deliberately), and global accesses are warp-uniform so the four samples run one
# at a time instead of four lanes at once.
#
# The MMIO-window version of the same workload is tests/test_mlp.sh and passes.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
OUT="$(mktemp)"
ERR="$(mktemp)"
trap 'rm -f "$LOG" "$OUT" "$ERR"' EXIT

python3 "$ROOT/tests/gen_gpu_kernel.py" \
    --emit-multi firmware/gpu_kernels/matmul_mem.S,firmware/gpu_kernels/relu_mem.S \
    --out-multi firmware/apps/gpu_mlp_mem_kernels.h >/dev/null \
    || { echo "kernel assembly failed" >&2; exit 2; }

TB_QUIET=8000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" mlp_mem_test > "$OUT" 2> "$ERR"
cat "$OUT" "$ERR" > "$LOG"

check_init "$LOG" "shared-memory mlp"

check_has '\[MLP\] shared-memory MLP: base=0x[0-9a-f]{8} kernels=[0-9]+\+[0-9]+ words, 4 samples' \
          'workload started over shared memory'
check_has '\[TEST\] sample0 PASS' 'sample 0 matched the scalar reference'
check_has '\[TEST\] sample1 PASS' 'sample 1 matched the scalar reference'
check_has '\[TEST\] sample2 PASS' 'sample 2 matched the scalar reference'
check_has '\[TEST\] sample3 PASS' 'sample 3 matched the scalar reference'
check_has '\[TEST\] shared_mem PASS' 'the whole workload ran with no staging window'
check_has '\[MLP\] gpu_cycles=[0-9]+ \(mmio-window version: 6278' \
          'cycle cost reported for comparison against the MMIO version'
check_has '\[TEST\] DONE' 'app signalled completion'

echo "--- cost, same workload two ways ---"
grep -a '^\[MLP\] gpu_cycles' "$LOG" | sed 's/^/  /'
echo "  (MMIO staging window version: 6278 cycles - from tests/test_mlp.sh)"

check_pass
