#!/usr/bin/env bash
# ============================================================================
# run_compliance.sh — riscv-arch-test runner for the Omni-RISC RV32IM CPU
# ============================================================================
# Builds each test once (non-selfcheck, sail-macros tohost termination), runs
# it on (a) ref_sim — a small independent RV32IM interpreter (the reference
# model) — and (b) the Verilator CPU harness (tb_compliance), then diffs the
# two signatures.
#
# Reference caveat: qemu-user cannot extract signatures (its RISC-V semihosting
# only implements SYS_EXIT and its gdb-stub is unreliable for memory reads),
# so the golden signatures come from ref_sim, a self-contained interpreter
# written from the ISA spec. See docs/compliance_results.md.
#
# Usage:
#   ./tests/run_compliance.sh [--suite rv32i|rv32im|all] [--test NAME]
# ============================================================================
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
ARCH_TEST="${PROJECT_ROOT}/tests/riscv-arch-test"
OMNI="${PROJECT_ROOT}/tests/omni-risc"
WORK="${PROJECT_ROOT}/tests/compliance_work"

CC=riscv64-linux-gnu-gcc
OBJCOPY=riscv64-linux-gnu-objcopy
NM=riscv64-linux-gnu-nm
REF_SIM="${WORK}/ref_sim"
DUT="${PROJECT_ROOT}/hardware/sim/obj_dir_tb_compliance/Vcpu_top"

SUITE="rv32im"   # rv32i (I/Zicsr/Zifencei) + rv32im (adds M) — same dirs
EXT_DIRS="I M Zicsr Zifencei"
TOTAL=0 PASSED=0 FAILED=0 SKIPPED=0

# Tests that exercise behavior the Omni-RISC architecture cannot provide.
# Zifencei-fence.i: self-modifying code — requires unified I/D memory; the CPU
# has split BRAMs, so SW to the code region never reaches the fetch stream.
skip_reason() {
    case "$1" in
        Zifencei-fence.i-00) echo "self-modifying code — needs unified I/D memory; CPU has split BRAMs" ;;
        *) echo "" ;;
    esac
}

info() { printf "\033[1;34m[INFO]\033[0m  %s\n" "$*"; }
ok()   { printf "\033[1;32m[PASS]\033[0m  %s\n" "$*"; }
fail() { printf "\033[1;31m[FAIL]\033[0m  %s\n" "$*"; }

while [ $# -gt 0 ]; do
    case "$1" in
        --test) TEST_FILTER="$2"; shift 2 ;;
        *) shift ;;
    esac
done

mkdir -p "${WORK}"
g++ -O2 -o "${REF_SIM}" "${OMNI}/ref_sim.cpp"

[ -x "${DUT}" ] || { fail "build the DUT harness first: cd hardware/sim && verilator ... (see tb_compliance.cpp)"; exit 1; }

echo "Omni-RISC RV32IM compliance — ref: ref_sim (interpreter), dut: Verilator CPU" > "${WORK}/results.log"

for DIR in ${EXT_DIRS}; do
    for TEST_SRC in "${ARCH_TEST}/tests/rv32i/${DIR}"/*.S; do
        NAME="$(basename "${TEST_SRC}" .S)"
        [ -z "${TEST_FILTER:-}" ] || [[ "${NAME}" == *"${TEST_FILTER}"* ]] || continue
        TOTAL=$((TOTAL + 1))
        REASON="$(skip_reason "${NAME}")"
        if [ -n "${REASON}" ]; then
            printf "\033[1;33m[SKIP]\033[0m  %s — %s\n" "${NAME}" "${REASON}"
            SKIPPED=$((SKIPPED + 1)); echo "SKIP ${NAME} (${REASON})" >> "${WORK}/results.log"; continue
        fi
        T="${WORK}/${NAME}"
        mkdir -p "${T}"

        if ! "${CC}" -march=rv32im_zicsr_zifencei -mabi=ilp32 -nostdlib -nostartfiles \
                -T "${OMNI}/link.ld" \
                -I"${ARCH_TEST}/tests/env" -I"${OMNI}" \
                -DTEST_FILE="\"${NAME}.S\"" -DXLEN=32 -DTEST_FLEN=32 \
                -o "${T}/${NAME}.elf" "${TEST_SRC}" 2> "${T}/compile.log"; then
            fail "${NAME} — compile error"; echo "FAIL ${NAME} (compile)" >> "${WORK}/results.log"; continue
        fi

        SIGUPD=$(grep -m1 "#define SIGUPD_COUNT" "${TEST_SRC}" | awk '{print $3}')
        PASS_ADDR=$("${NM}" "${T}/${NAME}.elf" | awk '$3=="write_tohost_pass"{print "0x"$1}')
        FAIL_ADDR=$("${NM}" "${T}/${NAME}.elf" | awk '$3=="write_tohost_fail"{print "0x"$1}')
        SIG_BEG=$("${NM}" "${T}/${NAME}.elf" | awk '$3=="begin_signature"{print "0x"$1}')
        TOHOST=$("${NM}" "${T}/${NAME}.elf" | awk '$3=="tohost"{print "0x"$1}')

        "${OBJCOPY}" -O verilog "${T}/${NAME}.elf" "${T}/${NAME}.hex"

        if ! "${REF_SIM}" "${T}/${NAME}.hex" "${PASS_ADDR}" "${FAIL_ADDR}" "${SIG_BEG}" "${SIGUPD}" "${T}/${NAME}.ref.sig" 2> "${T}/ref.log"; then
            fail "${NAME} — reference did not pass"; echo "FAIL ${NAME} (ref)" >> "${WORK}/results.log"; continue
        fi

        if ! "${DUT}" "+hex=${T}/${NAME}.hex" +entry=0x100000 "+pass=${PASS_ADDR}" "+fail=${FAIL_ADDR}" \
                "+tohost=${TOHOST}" "+sig=${SIG_BEG}" "+words=${SIGUPD}" "+out=${T}/${NAME}.dut.sig" \
                2> "${T}/dut.log"; then
            fail "${NAME} — DUT did not reach pass"; cat "${T}/dut.log"; echo "FAIL ${NAME} (dut)" >> "${WORK}/results.log"; continue
        fi

        if cmp -s "${T}/${NAME}.ref.sig" "${T}/${NAME}.dut.sig"; then
            ok "${NAME}"; PASSED=$((PASSED + 1)); echo "PASS ${NAME}" >> "${WORK}/results.log"
        else
            fail "${NAME} — signature mismatch"; echo "FAIL ${NAME} (sig)" >> "${WORK}/results.log"
        fi
    done
done

echo ""
info "Summary: ${PASSED}/${TOTAL} passed, ${FAILED} failed, ${SKIPPED} skipped"
[ "${FAILED}" -eq 0 ]
