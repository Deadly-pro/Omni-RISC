#!/usr/bin/env bash
# M2 gate — a GPU kernel reading and writing the CPU's own memory.
#
#   ./tests/test_gpu_mem.sh
#
# The kernel (firmware/gpu_kernels/mem_add.S) uses LDG/STG against the GPU's bus
# master port, so it addresses the same data_bram the CPU's loads and stores use.
# The app stages nothing: plain C stores into a volatile array, a plain C read of
# the result. If it passes, the GPU and the CPU share memory.
#
# Exit 0 = the kernel's stores landed in the CPU's array and the CPU read them.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"
LOG="$(mktemp)"
OUT="$(mktemp)"
ERR="$(mktemp)"
trap 'rm -f "$LOG" "$OUT" "$ERR"' EXIT

python3 "$ROOT/tests/gen_gpu_kernel.py" \
    --emit-multi firmware/gpu_kernels/mem_add.S \
    --out-multi firmware/apps/gpu_mem_kernel.h >/dev/null \
    || { echo "kernel assembly failed" >&2; exit 2; }

# Firmware transcript on stdout, harness diagnostics on stderr, kept apart so a
# heartbeat cannot split a firmware line (see tests/test_bench.sh).
TB_QUIET=8000000 TB_START_MARK='[TEST] READY' TB_PACE=line \
    "$ROOT/tests/run_app.sh" gpu_mem_test > "$OUT" 2> "$ERR"
cat "$OUT" "$ERR" > "$LOG"

check_init "$LOG" "gpu unified memory"

check_has '\[TEST\] gpu_mem base=0x[0-9a-f]{8} a=111 b=222' \
          'CPU staged its array with ordinary C stores (no MMIO window)'
check_has '\[TEST\] gpu_mem done=1 c=333 irq=1' \
          'the kernel added the CPU memory words and stored the result back into it'
check_has '\[TEST\] gpu_mem PASS' 'GPU memory access verified against the CPU-side value'
check_has '\[TEST\] gpu_mem_again PASS' 'a second launch saw the CPU newer values'
check_has '\[TEST\] SUMMARY 2/2' 'both families passed'
check_has '\[TEST\] DONE' 'app signalled completion'

echo "--- kernel's view of the CPU's array ---"
grep -a '^\[TEST\] gpu_mem ' "$LOG" | sed 's/^/  /'

check_pass
