#!/usr/bin/env bash
# run_app.sh — build a firmware app, stage it for the generic console TB, run it.
#
#   tests/run_app.sh <app> [session-file]
#
# Prints the firmware's UART TX (stdout) merged with the harness chatter
# (stderr) into whatever the caller redirects. Exit status:
#   0  the app reached its end marker, or stdin hit EOF and TX went quiet
#   1  the TB safety cap was hit — image hung or never printed the marker
#   2  the firmware build failed (build output is shown, not swallowed)
#
# Env passed through to build/TB:
#   APP_DEFS      extra CFLAGS for this app, e.g. APP_DEFS=-DWITH_CONV
#   TB_MAX TB_QUIET TB_PACE TB_END_MARK TB_HEARTBEAT TB_DEBUG TB_VCD
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="${1:?usage: run_app.sh <app> [session-file]}"
SESSION="${2:-}"
# The harness runs the TB from its own obj_dir, so a relative session path would
# stop resolving: make it absolute here.
if [ -n "$SESSION" ]; then
    SESSION="$(cd "$(dirname "$SESSION")" && pwd)/$(basename "$SESSION")"
fi
TB="soc/tb_soc_console"
OBJ="$ROOT/hardware/sim/obj_dir_tb_soc_console"

# 1) firmware — build chatter goes to STDERR: stdout belongs to the firmware's
# UART output, and a gate that byte-diffs it must not see "make: Entering
# directory ..." mixed in. A broken app must not fail quietly, either.
# APP_DEFS changes the compile command but not any file timestamp, so make would
# happily reuse an object built for a different gate mode (F3 runs the same app
# as burst and as line). Drop it whenever defines are supplied.
if [ -n "${APP_DEFS:-}" ]; then rm -f "$ROOT/firmware/apps/$APP.o"; fi

if ! make -C "$ROOT/firmware" APP="$APP" APP_DEFS="${APP_DEFS:-}" "$APP.hex" 1>&2; then
    echo "[RUN] firmware build FAILED (APP=$APP)" >&2
    exit 2
fi
[ -f "$ROOT/firmware/$APP.hex" ] || {
    echo "[RUN] $APP.hex not produced" >&2; exit 2; }

# 2) harness binary — build once, with a trivial cap (this is not a test run)
if [ ! -x "$OBJ/Vsoc_top" ]; then
    echo "[RUN] building $TB (first time only) ..." >&2
    TB_MAX=1000 "$ROOT/hardware/sim/run_sim.sh" "$TB" </dev/null >/dev/null 2>&1 || true
    [ -x "$OBJ/Vsoc_top" ] || { echo "[RUN] $TB failed to build" >&2; exit 2; }
fi

# 3) the image this TB boots
cp "$ROOT/firmware/$APP.hex" "$OBJ/program.hex"

# The SoC's GPU IMEM is $readmemh'd from gpu_demo.hex. Stage the demo kernel so
# Verilator does not print a "file not found" warning onto stdout - gates that
# byte-diff the UART output care. Set TB_GPU_IMAGE=none to leave IMEM stubbed
# (the F8 gate needs that to prove runtime loading).
if [ "${TB_GPU_IMAGE:-demo}" = "demo" ] && [ ! -f "$OBJ/gpu_demo.hex" ]; then
    python3 "$ROOT/scripts/gpu_asm.py" \
        "$ROOT/firmware/gpu_kernels/gpu_demo.S" -o "$OBJ/gpu_demo.hex" >/dev/null 2>&1 || true
fi

# 4) run
echo "[RUN] APP=$APP  TB=$TB  image=$(wc -c < "$OBJ/program.hex") hex bytes${APP_DEFS:+  APP_DEFS=$APP_DEFS}" >&2
cd "$OBJ" || exit 2
if [ -n "$SESSION" ]; then
    ./Vsoc_top < "$SESSION"
else
    ./Vsoc_top < /dev/null
fi
