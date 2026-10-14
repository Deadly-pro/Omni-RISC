#!/usr/bin/env bash
# M3 gate — the MLP computed over shared memory, and what that cost.
#
#   ./tests/test_mlp_mem.sh
#
# Same computation as tests/test_mlp.sh (y = relu(W2 . relu(W1 . x)), 4 samples,
# 4 launches each, verified per sample against a scalar reference) but with no
# staging window at all: the operands live in an ordinary C array and the kernels
# reach them with LDG/STG.
#
# It passes - and it is SLOWER, which is the result worth having:
#
#   15125 cycles vs the MMIO staging window's 6278 for the same four samples.
#
# The arithmetic is not mysterious: this workload makes ~224 global accesses
# (56 per sample: 16 LDG + 4 STG per matmul, 8 per relu), and the difference is
# ~8847 cycles - about 40 cycles per access. Each one is a bus transaction that
# has to arbitrate with a CPU that is polling *memory* in gpu_run_wait_irq's wait
# loop (mem_arbiter gives the CPU priority on purpose), whereas a scratchpad
# access costs one cycle and the MMIO window only costs the CPU two pbus stores to
# push a word. So "unified memory" is not automatically a win: it moves the cost
# from the CPU's staging loop into the GPU's access path.
#
# Concrete ways to fix that, in order of expected effect: have the wait loop poll
# only a peripheral so it stops competing for the memory bus; give the GPU
# priority (or exclusive ownership) while a kernel is running; and allow per-lane
# global addresses so four SIMT lanes can work in parallel again instead of the
# samples being serialized by the warp-uniform access rule.
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
