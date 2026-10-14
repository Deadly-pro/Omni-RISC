#!/usr/bin/env bash
# F8 gate — runtime GPU kernel loading (firmware track).
#
#   ./tests/test_gpu_loader.sh
#
# Two runs of the same app, differing only in whether the kernel words are pushed
# into the GPU over the load port. The RTL's GPU instruction memory is NOT
# pre-flashed for this gate (no gpu_demo.hex), so:
#
#   load=1  -> the load pointer advances, the kernel runs, the completion
#              interrupt arrives, and C = A + B is in the scratchpad
#   load=0  -> nothing to run, so no completion interrupt ever arrives and the
#              result never appears
#
# The second run is the interesting one: it is what rules out "the kernel was
# already in there".
#
# Verilator-only verification (this host has no Vivado).
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
. "$ROOT/tests/check.sh"

fail=0
OBJ="$ROOT/hardware/sim/obj_dir_tb_soc_console"

# The kernel words come from the real assembler, not from a hand-copied table.
python3 "$ROOT/tests/gen_gpu_kernel.py" >/dev/null || { echo "kernel assembly failed" >&2; exit 2; }

run_case()   # <load> <check-name> <imem-pat> <irq-pat> <res-pat> <res-label>
{
    local load="$1" name="$2" ipat="$3" qpat="$4" rpat="$5" rlabel="$6"
    local log
    log="$(mktemp)"

    # No pre-flashed kernel: whatever is in the run dir must not be the source of
    # the answer. (The RTL still names gpu_demo.hex for the flashed path.)
    rm -f "$OBJ/gpu_demo.hex"

    APP_DEFS="-DGPU_LOAD=$load" TB_GPU_IMAGE=none \
    TB_START_MARK='[TEST] READY' TB_PACE=line TB_QUIET=15000000 TB_MAX=800000000 \
        "$ROOT/tests/run_app.sh" gpu_loader_test > "$log" 2>&1

    check_init "$log" "$name"
    check_has "\\[TEST\\] gpu_load gate load=$load" 'app built for this case'
    check_has "$ipat" 'load pointer behaved as expected'
    check_has "$qpat" 'completion interrupt behaved as expected'
    check_has "$rpat" "$rlabel"
    check_has '\[TEST\] DONE' 'app signalled completion'
    check_pass || fail=1
    rm -f "$log"
}

# load=1: the kernel really is pushed, runs, interrupts, and computes.
run_case 1 "gpu load: kernel pushed over the load port" \
    '\[TEST\] gpu_load imem_addr=30' \
    '\[TEST\] gpu_load waited=1 handler=1 irq=1' \
    '\[TEST\] SUMMARY 3/3' \
    'all three families passed with the kernel loaded at run time'

# load=0: nothing pushed -> no interrupt, no result.
run_case 0 "gpu load: nothing pushed" \
    '\[TEST\] gpu_load imem_addr=0' \
    '\[TEST\] gpu_load waited=0 handler=0 irq=0' \
    '\[TEST\] SUMMARY 3/3' \
    'with no kernel loaded there was no completion and no result'

exit $fail
