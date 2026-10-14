#!/usr/bin/env bash
# Run the formal harnesses under hardware/formal/.
#
#   ./scripts/formal.sh              # every .sby
#   ./scripts/formal.sh mem_arbiter  # just one
#
# Bounded model checking with SymbiYosys: instead of simulating sequences someone
# thought of, the solver looks for ANY sequence up to the bound that violates the
# assertions in the harness. For a block whose whole job is mutual exclusion and
# honest acknowledgements - the memory arbiter between the CPU, the GPU and the one
# data_bram - that is a stronger statement than any testbench.
#
# The bound is finite, so this proves "no violation within N cycles", not "never".
# That is the honest reading and it is still much stronger than a directed test.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v sby >/dev/null 2>&1; then
    cat >&2 <<'EOF'
sby (SymbiYosys) is not installed.

  Arch / CachyOS :  yay -S symbiyosys     # or: pacman -S yosys-sby, if your repo has it
  source         :  https://github.com/YosysHQ/sby
  solver needed  :  sudo pacman -S z3     # any of z3/boolector/yices will do

Note: `yosys` alone is enough for scripts/yosys_area.sh; sby is only needed here.
EOF
    exit 2
fi

WANT="${1:-}"
found=0
rc=0

# Paths inside a .sby are relative to the directory sby is run FROM, so run from the
# repository root. (If a future sby resolves them relative to the .sby instead, the
# [script]/[files] paths there need a ../../ prefix.)
for f in hardware/formal/*.sby; do
    [ -e "$f" ] || continue
    name="$(basename "$f" .sby)"
    [ -n "$WANT" ] && [ "$name" != "$WANT" ] && continue
    found=1
    echo "=== formal: $name (bmc) ==="
    if sby -f "$f"; then
        echo "  PROVED within the bound"
    else
        echo "  FAILED - a counterexample is in hardware/formal/${name}.sby.d/engine_0/trace.vcd"
        rc=1
    fi
    echo
done

if [ "$found" -eq 0 ]; then
    echo "no matching .sby under hardware/formal/ (looked for '${WANT:-*}')" >&2
    exit 1
fi

echo "Reminder: a bound is not a proof. 'Proved' above means no violation was found"
echo "within the depth in each .sby, not that none exists."
exit $rc
