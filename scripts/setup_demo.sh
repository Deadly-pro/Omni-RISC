#!/usr/bin/env bash
# setup_demo.sh — one command from a fresh clone to a running demo.
#
#   ./scripts/setup_demo.sh          check deps -> build -> gate -> live console
#   ./scripts/setup_demo.sh --check  dependency check only (exit 1 if missing)
#   ./scripts/setup_demo.sh --no-run build + gate only (no interactive shell)
#
# The demo is scripts/console.sh: type into the FreeRTOS shell running on the
# RV32IM core inside Verilator, over a bit-accurate 115200 UART (tb_soc_shell
# feeds stdin at real framing — 434 clk/bit — so the RTL deserializer sees
# genuine UART waveforms).
#
# Everything the demo needs is checked here before anything is built. The
# RISC-V cross toolchain is the one package a fresh machine usually lacks.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
# Same default as firmware/Makefile; override with TOOLCHAIN_PREFIX=/path/prefix-
PREFIX="${TOOLCHAIN_PREFIX:-${PREFIX:-riscv64-linux-gnu-}}"
export PREFIX

if [ -t 1 ]; then
    RED='\033[0;31m'; GREEN='\033[0;32m'; YELLOW='\033[1;33m'
    CYAN='\033[0;36m'; BOLD='\033[1m'; RESET='\033[0m'
else
    RED=''; GREEN=''; YELLOW=''; CYAN=''; BOLD=''; RESET=''
fi

RUN=1
CHECK_ONLY=0
for arg in "$@"; do
    case "$arg" in
        --check)  CHECK_ONLY=1 ;;
        --no-run) RUN=0 ;;
        -h|--help) sed -n '2,10p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
        *) echo "unknown option: $arg (try --help)" >&2; exit 2 ;;
    esac
done

have() { command -v "$1" >/dev/null 2>&1; }

# version_ge A B -> true when A >= B (sort -V ordering)
version_ge() { [ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" = "$2" ]; }

MISSING=0
WARNED=0

hard() {  # hard <name> <command> <hint>
    if have "$2"; then
        printf '  %s%-22s%s %s\n' "$GREEN" "$1" "$RESET" "ok — $(command -v "$2")"
    else
        printf '  %s%-22s%s %s\n' "$RED" "$1" "$RESET" "MISSING"
        printf '      install: %s\n' "$3"
        MISSING=1
    fi
}

soft() {  # soft <name> <command> <why>
    if have "$2"; then
        printf '  %s%-22s%s %s\n' "$GREEN" "$1" "$RESET" "ok"
    else
        printf '  %s%-22s%s %s\n' "$YELLOW" "$1" "$RESET" "not installed — $3"
        WARNED=1
    fi
}

echo -e "${BOLD}Omni-RISC APU — demo setup${RESET} (${ROOT})"
echo
echo -e "${BOLD}Required:${RESET}"
hard verilator  verilator            "sudo pacman -S verilator"
hard python3    python3              "sudo pacman -S python"
hard "${PREFIX}gcc"     "${PREFIX}gcc"     "sudo pacman -S riscv64-linux-gnu-gcc"
hard "${PREFIX}objcopy" "${PREFIX}objcopy" "sudo pacman -S riscv64-linux-gnu-gcc"
hard "${PREFIX}as"      "${PREFIX}as"      "sudo pacman -S riscv64-linux-gnu-gcc"
echo
echo -e "${BOLD}Optional:${RESET}"
soft gtkwave    gtkwave              "needed only for run_sim.sh --wave"
soft asciinema  asciinema            "needed only to replay docs/shell_session.cast"
if python3 -c 'import PIL' >/dev/null 2>&1; then
    printf '  %s%-16s%s %s\n' "$GREEN" "Pillow" "$RESET" "ok"
else
    printf '  %s%-16s%s %s\n' "$YELLOW" "Pillow" "$RESET" \
        'not installed — needed only by scripts/make_shell_demo.py (sudo pacman -S python-pillow)'
    WARNED=1
fi
echo

if have verilator && ! version_ge "$(verilator --version | awk '{print $2}')" 5.0; then
    echo -e "${YELLOW}WARNING:${RESET} Verilator $(verilator --version | awk '{print $2}') < 5.0 — the TBs need >= 5.0"
fi

if [ "$MISSING" -ne 0 ]; then
    echo -e "${RED}${BOLD}Missing required packages.${RESET} Install them and re-run, e.g. on Arch/CachyOS:"
    echo
    echo "    sudo pacman -S verilator riscv64-linux-gnu-gcc python python-pillow"
    echo
    exit 1
fi
[ "$WARNED" -ne 0 ] && echo -e "${YELLOW}Optional packages missing — the demo still runs.${RESET}"
echo -e "${GREEN}All required packages present.${RESET}"

if [ "$CHECK_ONLY" -eq 1 ]; then
    echo "--check: nothing built."
    exit 0
fi

echo
echo -e "${BOLD}==> 1/3 firmware:${RESET} make -C firmware APP=shell (FreeRTOS + shell app)"
# Not silenced: a toolchain problem has to be visible, not swallowed.
make -C "$ROOT/firmware" APP=shell shell.hex
[ -f "$ROOT/firmware/shell.hex" ] || { echo -e "${RED}ERROR:${RESET} firmware/shell.hex was not produced"; exit 1; }

echo
echo -e "${BOLD}==> 2/3 testbench:${RESET} building tb_soc_shell (first build takes a few minutes)"
# Also stages program.hex + gpu_demo.hex into obj_dir_tb_soc_shell, and boots
# once with stdin at EOF so a broken image shows up here rather than mid-demo.
"$ROOT/hardware/sim/run_sim.sh" soc/tb_soc_shell < /dev/null

echo
echo -e "${BOLD}==> 3/3 gate:${RESET} tests/test_shell.sh (scripted session, 18 checks)"
"$ROOT/tests/test_shell.sh"

echo
if [ "$RUN" -eq 1 ] && [ -t 0 ] && [ -t 1 ]; then
    echo -e "${GREEN}${BOLD}Demo ready — launching the live console (Ctrl-C quits).${RESET}"
    echo
    exec "$ROOT/scripts/console.sh"
else
    echo -e "${GREEN}${BOLD}Demo ready.${RESET}"
    [ "$RUN" -eq 1 ] && echo "stdin/stdout is not a terminal — skipping the interactive console."
    echo
    echo "  ./tests/test_shell.sh   # scripted session (no typing needed)"
    echo "  ./scripts/console.sh    # live terminal: type at the APU (Ctrl-C quits)"
fi
