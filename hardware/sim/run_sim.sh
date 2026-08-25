#!/usr/bin/env bash
# =============================================================================
# run_sim.sh — Verilator simulation driver for Omni-RISC APU
# =============================================================================
#
# Usage:
#   ./run_sim.sh <testbench>            # e.g., cpu/tb_alu
#   ./run_sim.sh <testbench> --wave     # also open GTKWave after sim
#   ./run_sim.sh --help
#
# The script:
#   1) Locates the C++ testbench at hardware/sim/<testbench>.cpp
#   2) Locates the DUT Verilog at hardware/rtl/<dut>.v  (derived from tb name)
#   3) Compiles with Verilator (--cc --exe --trace --Wall)
#   4) Runs the simulation binary
#   5) Reports PASS / FAIL based on exit code
#   6) Optionally opens GTKWave on the .vcd trace
#
# Exit codes:
#   0 — all tests passed
#   1 — one or more tests failed or build error
#   2 — usage / environment error
# =============================================================================

set -euo pipefail

# ---- Colour helpers (disable when stdout is not a terminal) -----------------
if [ -t 1 ]; then
    RED='\033[0;31m'
    GREEN='\033[0;32m'
    YELLOW='\033[1;33m'
    CYAN='\033[0;36m'
    BOLD='\033[1m'
    RESET='\033[0m'
else
    RED='' GREEN='' YELLOW='' CYAN='' BOLD='' RESET=''
fi

# ---- Resolve project root (two levels up from this script) ------------------
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
SIM_DIR="$PROJECT_ROOT/hardware/sim"
RTL_DIR="$PROJECT_ROOT/hardware/rtl"

# ---- Default configuration ---------------------------------------------------
OPEN_WAVE=0
VERILATOR_FLAGS="--cc --exe --trace --Wall -Wno-UNUSED -Wno-UNDRIVEN -Wno-PINCONNECTEMPTY -Wno-EOFNEWLINE -Wno-BLKSEQ"
# Extra include paths: all RTL subdirectories so `include works
RTL_INCLUDES=""
while IFS= read -r -d '' dir; do
    RTL_INCLUDES="$RTL_INCLUDES -I$dir"
done < <(find "$RTL_DIR" -type d -print0 2>/dev/null || true)

# ---- Build output directory --------------------------------------------------
BUILD_DIR="$SIM_DIR/obj_dir"

# =============================================================================
# Usage / help
# =============================================================================
usage() {
    cat <<EOF
${BOLD}Omni-RISC Verilator Simulation Runner${RESET}

${CYAN}USAGE${RESET}
    $(basename "$0") <testbench_path> [OPTIONS]

${CYAN}ARGUMENTS${RESET}
    testbench_path    Relative path under hardware/sim/ without extension.
                      Example: cpu/tb_alu  →  hardware/sim/cpu/tb_alu.cpp

${CYAN}OPTIONS${RESET}
    --wave            Open GTKWave on the generated .vcd after simulation
    --help, -h        Show this help message

${CYAN}EXAMPLES${RESET}
    ./run_sim.sh cpu/tb_alu
    ./run_sim.sh cpu/tb_decoder --wave
    ./run_sim.sh cpu/tb_cache

${CYAN}NOTES${RESET}
    • Requires Verilator ≥ 4.0 and a C++17 compiler.
    • VCD waveforms are written to hardware/sim/<testbench>.vcd.
    • GTKWave is optional; install it separately if --wave is desired.
EOF
    exit 0
}

# =============================================================================
# Argument parsing
# =============================================================================
TB_PATH=""

for arg in "$@"; do
    case "$arg" in
        --help|-h)
            usage
            ;;
        --wave)
            OPEN_WAVE=1
            ;;
        -*)
            echo -e "${RED}ERROR:${RESET} Unknown option: $arg"
            echo "Run with --help for usage."
            exit 2
            ;;
        *)
            if [ -z "$TB_PATH" ]; then
                TB_PATH="$arg"
            else
                echo -e "${RED}ERROR:${RESET} Multiple testbench paths given."
                exit 2
            fi
            ;;
    esac
done

if [ -z "$TB_PATH" ]; then
    echo -e "${RED}ERROR:${RESET} No testbench path provided."
    echo "Run with --help for usage."
    exit 2
fi

# =============================================================================
# Environment checks
# =============================================================================
if ! command -v verilator &>/dev/null; then
    echo -e "${RED}ERROR:${RESET} Verilator is not installed or not in PATH."
    echo ""
    echo "Install Verilator:"
    echo "  Ubuntu/Debian:  sudo apt install verilator"
    echo "  Fedora:         sudo dnf install verilator"
    echo "  macOS (brew):   brew install verilator"
    echo "  From source:    https://verilator.org/guide/latest/install.html"
    exit 2
fi

VERILATOR_VER=$(verilator --version | head -1)
echo -e "${CYAN}Verilator:${RESET} $VERILATOR_VER"

# =============================================================================
# Locate testbench and DUT files
# =============================================================================
TB_CPP="$SIM_DIR/${TB_PATH}.cpp"
if [ ! -f "$TB_CPP" ]; then
    echo -e "${RED}ERROR:${RESET} Testbench not found: $TB_CPP"
    exit 2
fi

# Derive DUT name from testbench filename: tb_alu → alu
TB_BASENAME="$(basename "$TB_PATH")"
DUT_NAME="${TB_BASENAME#tb_}"

# SoC testbenches all use soc_top as DUT
if [[ "$TB_BASENAME" == tb_soc_* ]]; then
    DUT_NAME="soc_top"
fi

# CPU testbenches that test specific modules (not cpu_top)
if [[ "$TB_BASENAME" == tb_cache ]]; then
    DUT_NAME="l1_dcache"
fi

if [[ "$TB_BASENAME" == tb_icache ]]; then
    DUT_NAME="l1_icache"
fi

# Compliance testbench uses cpu_top
if [[ "$TB_BASENAME" == tb_compliance ]]; then
    DUT_NAME="cpu_top"
fi

# UART RX unit testbench drives the uart module directly (no CPU)
if [[ "$TB_BASENAME" == tb_uart_rx ]]; then
    DUT_NAME="uart"
fi

# Dual-core testbenches use dual_core_top
if [[ "$TB_BASENAME" == tb_dual_* ]] || [[ "$TB_BASENAME" == tb_litmus_* ]]; then
    DUT_NAME="dual_core_top"
fi

# GPU integration testbenches (tb_gpu_top_<x>) use gpu_top
if [[ "$TB_BASENAME" == tb_gpu_top_* ]]; then
    DUT_NAME="gpu_top"
fi

# Search for the DUT Verilog file anywhere under RTL
DUT_VERILOG=""
while IFS= read -r -d '' vfile; do
    DUT_VERILOG="$vfile"
    break
done < <(find "$RTL_DIR" -name "${DUT_NAME}.v" -print0 2>/dev/null)

if [ -z "$DUT_VERILOG" ]; then
    # Also check for .sv extension
    while IFS= read -r -d '' vfile; do
        DUT_VERILOG="$vfile"
        break
    done < <(find "$RTL_DIR" -name "${DUT_NAME}.sv" -print0 2>/dev/null)
fi

if [ -z "$DUT_VERILOG" ]; then
    echo -e "${YELLOW}WARNING:${RESET} DUT Verilog file '${DUT_NAME}.v' not found under $RTL_DIR"
    echo "         The build will likely fail until the RTL is written."
    echo "         Searched for: ${DUT_NAME}.v / ${DUT_NAME}.sv"
    # Still attempt the build — Verilator will give a clearer error
    DUT_VERILOG="$RTL_DIR/${DUT_NAME}.v"
fi

echo -e "${CYAN}Testbench:${RESET}  $TB_CPP"
echo -e "${CYAN}DUT:${RESET}        $DUT_VERILOG"

# =============================================================================
# VCD output path
# =============================================================================
VCD_DIR="$SIM_DIR/waves"
mkdir -p "$VCD_DIR"
VCD_FILE="$VCD_DIR/${TB_BASENAME}.vcd"

# =============================================================================
# Compile with Verilator
# =============================================================================
echo ""
echo -e "${BOLD}--- Compiling with Verilator ---${RESET}"

# Unique obj_dir per testbench to allow parallel builds
OBJ_DIR="$SIM_DIR/obj_dir_${TB_BASENAME}"
mkdir -p "$OBJ_DIR"

# Only tb_soc_rtos_q probes internal registers (BADREDIR/JUNK diagnostics);
# --public-flat-rw otherwise bloats every model (256KB BRAMs exposed → the
# compliance harness segfaults from the huge generated C++).
PUB_FLAT=""
if [[ "$TB_BASENAME" == tb_soc_rtos_q ]]; then PUB_FLAT="--public-flat-rw"; fi

# shellcheck disable=SC2086
if ! verilator $VERILATOR_FLAGS \
        $RTL_INCLUDES \
        --Mdir "$OBJ_DIR" \
        --top-module "$DUT_NAME" \
        $PUB_FLAT \
        -CFLAGS "-std=c++17 -DVCD_FILE=\\\"$VCD_FILE\\\"" \
        "$DUT_VERILOG" \
        "$TB_CPP" 2>&1; then
    echo -e "${RED}COMPILE FAILED${RESET}"
    exit 1
fi

echo -e "${BOLD}--- Building simulation binary ---${RESET}"
if ! make -C "$OBJ_DIR" -f "V${DUT_NAME}.mk" -j"$(nproc)" 2>&1; then
    echo -e "${RED}BUILD FAILED${RESET}"
    exit 1
fi

SIM_BIN="$OBJ_DIR/V${DUT_NAME}"
if [ ! -x "$SIM_BIN" ]; then
    echo -e "${RED}ERROR:${RESET} Simulation binary not found: $SIM_BIN"
    exit 1
fi

# Stage firmware images: the RTL BRAMs `$readmemh` from the run directory
# (instr_bram defaults to "program.hex"). Dual-core spinlock TBs need the
# spinlock image as program.hex. That hex is a gitignored build artifact, so
# assemble it on demand from apps/spinlock.S (C-style `//` comments stripped
# for GNU as).
if [[ "$TB_BASENAME" == tb_dual_spin ]]; then
    SPIN_S="$PROJECT_ROOT/firmware/apps/spinlock.S"
    SPIN_HEX="$PROJECT_ROOT/firmware/spinlock.hex"
    if [ ! -f "$SPIN_HEX" ] || [ "$SPIN_S" -nt "$SPIN_HEX" ]; then
        sed 's#//.*$##' "$SPIN_S" > /tmp/omni_spin.S
        riscv64-linux-gnu-as -march=rv32ima -o /tmp/omni_spin.o /tmp/omni_spin.S \
            && riscv64-linux-gnu-objcopy -O binary /tmp/omni_spin.o /tmp/omni_spin.bin \
            && python3 "$PROJECT_ROOT/firmware/elf_to_hex.py" /tmp/omni_spin.bin "$SPIN_HEX"
    fi
    cp "$SPIN_HEX" "$OBJ_DIR/program.hex"
fi

# FreeRTOS bring-up test: build the rtos app and stage it as program.hex.
if [[ "$TB_BASENAME" == tb_soc_rtos ]]; then
    make -C "$PROJECT_ROOT/firmware" APP=rtos rtos.hex >/dev/null 2>&1
    cp "$PROJECT_ROOT/firmware/rtos.hex" "$OBJ_DIR/program.hex"
fi

# FreeRTOS queue test: build the rtos_q app and stage it as program.hex.
if [[ "$TB_BASENAME" == tb_soc_rtos_q ]]; then
    make -C "$PROJECT_ROOT/firmware" APP=rtos_q rtos_q.hex >/dev/null 2>&1
    cp "$PROJECT_ROOT/firmware/rtos_q.hex" "$OBJ_DIR/program.hex"
fi

# FreeRTOS ISR-semaphore test (CLINT msip -> software interrupt): rtos_sem app.
if [[ "$TB_BASENAME" == tb_soc_rtos_sem ]]; then
    make -C "$PROJECT_ROOT/firmware" APP=rtos_sem rtos_sem.hex >/dev/null 2>&1
    cp "$PROJECT_ROOT/firmware/rtos_sem.hex" "$OBJ_DIR/program.hex"
fi

# FreeRTOS scheduler-metrics test (ISR latency / ctx-switch / tick jitter).
if [[ "$TB_BASENAME" == tb_soc_rtos_metrics ]]; then
    make -C "$PROJECT_ROOT/firmware" APP=rtos_metrics rtos_metrics.hex >/dev/null 2>&1
    cp "$PROJECT_ROOT/firmware/rtos_metrics.hex" "$OBJ_DIR/program.hex"
fi

# UART RX integration test: echo app (rx -> tx round trip).
if [[ "$TB_BASENAME" == tb_soc_uart_rx ]]; then
    make -C "$PROJECT_ROOT/firmware" APP=uart_echo uart_echo.hex >/dev/null 2>&1
    cp "$PROJECT_ROOT/firmware/uart_echo.hex" "$OBJ_DIR/program.hex"
fi

# APU GPU-dispatch test: stage the CPU firmware (benchmark_gpu) as program.hex
# and the flashed GPU kernel as gpu_demo.hex (soc_top's gpu_top IMEM_FILE).
if [[ "$TB_BASENAME" == tb_soc_gpu ]]; then
    # CPU firmware: build if absent, stage as program.hex
    make -C "$PROJECT_ROOT/firmware" APP=benchmark_gpu >/dev/null 2>&1
    cp "$PROJECT_ROOT/firmware/benchmark_gpu.hex" "$OBJ_DIR/program.hex"
    # GPU kernel: assemble on demand (firmware `make clean` nukes any *.hex)
    python3 "$PROJECT_ROOT/scripts/gpu_asm.py" \
        "$PROJECT_ROOT/firmware/gpu_kernels/gpu_demo.S" -o "$OBJ_DIR/gpu_demo.hex" \
        >/dev/null
fi

# =============================================================================
# Run simulation
# =============================================================================
echo ""
echo -e "${BOLD}--- Running simulation: $TB_BASENAME ---${RESET}"
echo ""

SIM_EXIT=0
# Run from OBJ_DIR so that program.hex in that directory is found
if (cd "$OBJ_DIR" && "$SIM_BIN" 2>&1); then
    echo ""
    echo -e "${GREEN}${BOLD}========================================${RESET}"
    echo -e "${GREEN}${BOLD}  SIMULATION PASSED  ✓${RESET}"
    echo -e "${GREEN}${BOLD}========================================${RESET}"
else
    SIM_EXIT=$?
    echo ""
    echo -e "${RED}${BOLD}========================================${RESET}"
    echo -e "${RED}${BOLD}  SIMULATION FAILED  ✗  (exit=$SIM_EXIT)${RESET}"
    echo -e "${RED}${BOLD}========================================${RESET}"
fi

# =============================================================================
# VCD waveform viewing
# =============================================================================
if [ -f "$VCD_FILE" ]; then
    echo -e "${CYAN}VCD trace:${RESET} $VCD_FILE"
    if [ "$OPEN_WAVE" -eq 1 ]; then
        if command -v gtkwave &>/dev/null; then
            echo "Opening GTKWave..."
            gtkwave "$VCD_FILE" &
        else
            echo -e "${YELLOW}WARNING:${RESET} GTKWave not found. Install with:"
            echo "  sudo apt install gtkwave"
        fi
    fi
else
    echo -e "${YELLOW}NOTE:${RESET} No VCD file generated at $VCD_FILE"
fi

echo ""
exit $SIM_EXIT
