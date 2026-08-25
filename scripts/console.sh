#!/usr/bin/env bash
# console.sh — live serial terminal into the simulated APU.
#
# Puts the real terminal into raw mode (character-at-a-time, no local echo)
# and attaches it to tb_soc_shell's stdin/stdout, so typing behaves like a
# minicom session over the bit-accurate UART: the RTL deserializer sees real
# framing, the shell does the echo and line editing, Ctrl-C quits.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

# build once up front (compile noise would garble the session)
"$ROOT/hardware/sim/run_sim.sh" soc/tb_soc_shell < /dev/null > /dev/null 2>&1
BIN="$ROOT/hardware/sim/obj_dir_tb_soc_shell/Vsoc_top"
[ -x "$BIN" ] || { echo "build failed"; exit 1; }
cp "$ROOT/firmware/shell.hex" "$ROOT/hardware/sim/obj_dir_tb_soc_shell/program.hex"

restore() { stty "$SAVED" 2>/dev/null; }
SAVED=$(stty -g)
trap restore EXIT INT TERM
stty raw -echo

cd "$ROOT/hardware/sim/obj_dir_tb_soc_shell"
"$BIN"          # foreground (not exec) so the EXIT trap restores the tty
