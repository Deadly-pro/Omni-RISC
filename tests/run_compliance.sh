#!/usr/bin/env bash
# ============================================================================
# run_compliance.sh — RISC-V Architecture Compliance Test Runner
# ============================================================================
#
# Runs the riscv-arch-test compliance suite (rv32i + rv32im) against the
# Omni-RISC APU Verilator simulation and reports per-test PASS/FAIL.
#
# Requirements:
#   - riscv32-unknown-elf-gcc   (RISC-V cross-compiler toolchain)
#   - riscv32-unknown-elf-objcopy
#   - Verilator simulation binary for the CPU (built separately)
#   - git (for cloning riscv-arch-test)
#   - diff, xxd (standard utilities)
#
# Assumptions:
#   - The CPU Verilator simulation binary is located at:
#       $PROJECT_ROOT/hardware/sim/cpu/obj_dir/Vcpu_top
#   - The simulation binary accepts a .hex file as its first argument.
#   - On exit, the simulation writes the signature memory region to a file
#     called "DUT-soc_top.signature" in the current working directory.
#   - The reference signatures are provided by riscv-arch-test.
#
# Usage:
#   ./tests/run_compliance.sh [--clean] [--suite rv32i|rv32im|all]
#
# ============================================================================

set -euo pipefail

# ============================================================================
# Configuration
# ============================================================================

# Project root: one level up from this script's directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Where we clone / find the riscv-arch-test repo
ARCH_TEST_DIR="${PROJECT_ROOT}/tests/riscv-arch-test"

# RISC-V cross-toolchain prefix
RISCV_PREFIX="${RISCV_PREFIX:-riscv32-unknown-elf-}"

# Verilator simulation binary path
SIM_BIN="${PROJECT_ROOT}/hardware/sim/cpu/obj_dir/Vcpu_top"

# Working directory for compilation and intermediate files
WORK_DIR="${PROJECT_ROOT}/tests/compliance_work"

# Linker script for compliance tests.
# The tests expect a specific memory layout; adjust as needed.
LINK_SCRIPT="${PROJECT_ROOT}/tests/compliance_link.ld"

# Results log
RESULTS_LOG="${WORK_DIR}/results.log"

# Suites to run (space-separated)
SUITES="rv32i rv32im"

# Counters
TOTAL=0
PASSED=0
FAILED=0
SKIPPED=0

# ============================================================================
# Helper functions
# ============================================================================

# Print a coloured status message
info()  { printf "\033[1;34m[INFO]\033[0m  %s\n" "$*"; }
ok()    { printf "\033[1;32m[PASS]\033[0m  %s\n" "$*"; }
fail()  { printf "\033[1;31m[FAIL]\033[0m  %s\n" "$*"; }
warn()  { printf "\033[1;33m[WARN]\033[0m  %s\n" "$*"; }
skip()  { printf "\033[1;33m[SKIP]\033[0m  %s\n" "$*"; }

# Check that a required tool is on PATH
require_tool() {
    local tool="$1"
    if ! command -v "$tool" &>/dev/null; then
        fail "Required tool not found: $tool"
        echo "       Please install it and ensure it is on your PATH."
        exit 1
    fi
}

# ============================================================================
# Argument parsing
# ============================================================================

DO_CLEAN=0
for arg in "$@"; do
    case "$arg" in
        --clean)
            DO_CLEAN=1
            ;;
        --suite)
            shift
            case "${1:-}" in
                rv32i)  SUITES="rv32i" ;;
                rv32im) SUITES="rv32im" ;;
                all)    SUITES="rv32i rv32im" ;;
                *)      echo "Unknown suite: ${1:-}"; exit 1 ;;
            esac
            ;;
    esac
done

# ============================================================================
# Pre-flight checks
# ============================================================================

info "Omni-RISC APU — RISC-V Compliance Test Runner"
info "Project root: ${PROJECT_ROOT}"
info "Suites:       ${SUITES}"
echo ""

# Verify required tools
require_tool "${RISCV_PREFIX}gcc"
require_tool "${RISCV_PREFIX}objcopy"
require_tool git
require_tool diff

# Verify simulation binary exists
if [ ! -x "${SIM_BIN}" ]; then
    fail "Simulation binary not found or not executable: ${SIM_BIN}"
    echo "       Build it first with:"
    echo "         cd ${PROJECT_ROOT}/hardware/sim/cpu"
    echo "         verilator --cc cpu_top.v --exe tb_cpu_top.cpp"
    echo "         make -C obj_dir -f Vcpu_top.mk"
    exit 1
fi

# ============================================================================
# Clone riscv-arch-test if not present
# ============================================================================

if [ ! -d "${ARCH_TEST_DIR}" ]; then
    info "Cloning riscv-arch-test repository..."
    git clone --depth 1 \
        https://github.com/riscv-non-isa/riscv-arch-test.git \
        "${ARCH_TEST_DIR}"
else
    info "riscv-arch-test already present at ${ARCH_TEST_DIR}"
fi

# ============================================================================
# Clean previous work if requested
# ============================================================================

if [ "${DO_CLEAN}" -eq 1 ]; then
    info "Cleaning previous work directory..."
    rm -rf "${WORK_DIR}"
fi

mkdir -p "${WORK_DIR}"

# ============================================================================
# Generate a minimal linker script if one doesn't exist
# ============================================================================

if [ ! -f "${LINK_SCRIPT}" ]; then
    warn "Linker script not found at ${LINK_SCRIPT}. Creating a default one."
    cat > "${LINK_SCRIPT}" << 'LDSCRIPT'
/*
 * Minimal linker script for RISC-V compliance tests.
 *
 * Memory map (adjust to match your SoC):
 *   0x00000000 - 0x0000FFFF : Instruction memory (64 KB)
 *   0x00010000 - 0x0001FFFF : Data memory (64 KB)
 *   0x0001FF00 - 0x0001FFFF : Signature region (256 bytes)
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    IMEM (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
    DMEM (rwx) : ORIGIN = 0x00010000, LENGTH = 64K
}

SECTIONS
{
    .text : { *(.text.init) *(.text*) } > IMEM

    .data : {
        *(.data*)
        *(.rodata*)
        *(.sdata*)
    } > DMEM

    .bss : {
        *(.bss*)
        *(.sbss*)
    } > DMEM

    /* Signature region — the simulation dumps this on exit */
    .signature : ALIGN(16) {
        begin_signature = .;
        *(.signature*)
        end_signature = .;
    } > DMEM

    _end = .;
}
LDSCRIPT
    info "Default linker script created at ${LINK_SCRIPT}"
fi

# ============================================================================
# Run tests
# ============================================================================

# Initialise results log
echo "Omni-RISC APU Compliance Test Results" > "${RESULTS_LOG}"
echo "Date: $(date -u '+%Y-%m-%d %H:%M:%S UTC')" >> "${RESULTS_LOG}"
echo "======================================" >> "${RESULTS_LOG}"

for SUITE in ${SUITES}; do
    info "================================================================"
    info "Running suite: ${SUITE}"
    info "================================================================"

    # Path to the test sources in riscv-arch-test
    TEST_SRC_DIR="${ARCH_TEST_DIR}/riscv-test-suite/${SUITE}/src"
    REF_DIR="${ARCH_TEST_DIR}/riscv-test-suite/${SUITE}/references"

    if [ ! -d "${TEST_SRC_DIR}" ]; then
        warn "Test source directory not found: ${TEST_SRC_DIR}"
        warn "Skipping suite ${SUITE}"
        continue
    fi

    # Create per-suite work directory
    SUITE_WORK="${WORK_DIR}/${SUITE}"
    mkdir -p "${SUITE_WORK}"

    # Iterate over every .S assembly source in the suite
    for TEST_SRC in "${TEST_SRC_DIR}"/*.S; do
        # Extract test name (e.g., "ADD" from "ADD.S")
        TEST_NAME="$(basename "${TEST_SRC}" .S)"
        TOTAL=$((TOTAL + 1))

        info "--- Test: ${SUITE}/${TEST_NAME} ---"

        TEST_WORK="${SUITE_WORK}/${TEST_NAME}"
        mkdir -p "${TEST_WORK}"

        # ------------------------------------------------------------------
        # Step 1: Compile the test
        # ------------------------------------------------------------------
        ELF_FILE="${TEST_WORK}/${TEST_NAME}.elf"
        HEX_FILE="${TEST_WORK}/${TEST_NAME}.hex"

        # Include paths for the compliance test macros
        INCLUDE_DIRS=(
            "-I${ARCH_TEST_DIR}/riscv-test-suite/env"
            "-I${ARCH_TEST_DIR}/riscv-target/omni-risc/model_test.h"
        )
        # If the target header doesn't exist, use a generic include path
        if [ ! -d "${ARCH_TEST_DIR}/riscv-target/omni-risc" ]; then
            INCLUDE_DIRS=(
                "-I${ARCH_TEST_DIR}/riscv-test-suite/env"
            )
        fi

        if ! "${RISCV_PREFIX}gcc" \
                -march=rv32im -mabi=ilp32 \
                -nostdlib -nostartfiles \
                -T "${LINK_SCRIPT}" \
                "${INCLUDE_DIRS[@]}" \
                -DXLEN=32 \
                -o "${ELF_FILE}" \
                "${TEST_SRC}" 2>"${TEST_WORK}/compile.log"; then
            skip "${SUITE}/${TEST_NAME} — compilation failed"
            cat "${TEST_WORK}/compile.log" 2>/dev/null || true
            SKIPPED=$((SKIPPED + 1))
            echo "SKIP  ${SUITE}/${TEST_NAME}  (compile error)" >> "${RESULTS_LOG}"
            continue
        fi

        # ------------------------------------------------------------------
        # Step 2: Convert ELF to flat hex ($readmemh format)
        # ------------------------------------------------------------------
        # First create a raw binary, then convert to hex
        BIN_FILE="${TEST_WORK}/${TEST_NAME}.bin"
        "${RISCV_PREFIX}objcopy" -O binary "${ELF_FILE}" "${BIN_FILE}"

        # Convert binary to hex: one 32-bit word per line, big-endian hex
        xxd -e -g4 -c4 "${BIN_FILE}" | awk '{print $2}' > "${HEX_FILE}"

        # ------------------------------------------------------------------
        # Step 3: Run through Verilator simulation
        # ------------------------------------------------------------------
        DUT_SIG="${TEST_WORK}/DUT-soc_top.signature"

        if ! timeout 30 "${SIM_BIN}" "+hex=${HEX_FILE}" \
                2>"${TEST_WORK}/sim_stderr.log" \
                1>"${TEST_WORK}/sim_stdout.log"; then
            # Non-zero exit could mean test completed (some sims use exit(1)
            # for ECALL). Check if signature file was produced.
            true
        fi

        # Move signature file if the sim wrote it to the current directory
        if [ -f "DUT-soc_top.signature" ]; then
            mv "DUT-soc_top.signature" "${DUT_SIG}"
        fi

        # ------------------------------------------------------------------
        # Step 4: Compare signature against reference
        # ------------------------------------------------------------------
        REF_SIG="${REF_DIR}/${TEST_NAME}.reference_output"

        if [ ! -f "${DUT_SIG}" ]; then
            fail "${SUITE}/${TEST_NAME} — no signature file produced"
            FAILED=$((FAILED + 1))
            echo "FAIL  ${SUITE}/${TEST_NAME}  (no signature)" >> "${RESULTS_LOG}"
            continue
        fi

        if [ ! -f "${REF_SIG}" ]; then
            skip "${SUITE}/${TEST_NAME} — no reference signature found"
            SKIPPED=$((SKIPPED + 1))
            echo "SKIP  ${SUITE}/${TEST_NAME}  (no reference)" >> "${RESULTS_LOG}"
            continue
        fi

        # Compare (ignoring trailing whitespace / blank lines)
        if diff -qBw "${DUT_SIG}" "${REF_SIG}" >/dev/null 2>&1; then
            ok "${SUITE}/${TEST_NAME}"
            PASSED=$((PASSED + 1))
            echo "PASS  ${SUITE}/${TEST_NAME}" >> "${RESULTS_LOG}"
        else
            fail "${SUITE}/${TEST_NAME} — signature mismatch"
            # Show the diff for debugging
            diff -u "${REF_SIG}" "${DUT_SIG}" | head -30 || true
            FAILED=$((FAILED + 1))
            echo "FAIL  ${SUITE}/${TEST_NAME}  (sig mismatch)" >> "${RESULTS_LOG}"
        fi
    done
done

# ============================================================================
# Summary
# ============================================================================

echo ""
echo "======================================"
info "Compliance Test Summary"
echo "======================================"
echo "  Total:   ${TOTAL}"
echo "  Passed:  ${PASSED}"
echo "  Failed:  ${FAILED}"
echo "  Skipped: ${SKIPPED}"
echo "======================================"

# Append summary to log
{
    echo ""
    echo "======================================"
    echo "Summary"
    echo "  Total:   ${TOTAL}"
    echo "  Passed:  ${PASSED}"
    echo "  Failed:  ${FAILED}"
    echo "  Skipped: ${SKIPPED}"
    echo "======================================"
} >> "${RESULTS_LOG}"

info "Detailed results: ${RESULTS_LOG}"

# Exit with failure if any test failed
if [ "${FAILED}" -gt 0 ]; then
    exit 1
fi

exit 0
