#!/usr/bin/env bash
# Independent area estimate with Yosys — a SECOND toolchain, no Vivado.
#
#   ./scripts/yosys_area.sh
#
# Why this exists: with no board and no Vivado, both "it synthesises" and "it is
# about this big" are otherwise single-sourced claims. Verilator compiles the RTL
# but does not map it to hardware; this maps the same RTL to Xilinx 7-series
# primitives with an independent implementation and prints an area breakdown.
#
# WHAT THIS IS NOT: Vivado's numbers. Yosys infers memories, DSPs and carry chains
# differently, so its LUT/FF/BRAM counts are a second opinion and a sanity check,
# NOT interchangeable with the Vivado figures in RESULTS.md. There is no timing
# here either - Yosys' free flow cannot do the Artix-7 timing analysis.
set -u
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

if ! command -v yosys >/dev/null 2>&1; then
    cat >&2 <<'EOF'
yosys is not installed.

  Arch / CachyOS :  sudo pacman -S yosys
  Debian / Ubuntu:  sudo apt install yosys
  otherwise      :  https://github.com/YosysHQ/yosys

Then re-run ./scripts/yosys_area.sh
EOF
    exit 2
fi

# Every RTL source, the same set run_sim.sh compiles.
mapfile -t FILES < <(find hardware/rtl -name '*.v' | sort)
if [ "${#FILES[@]}" -eq 0 ]; then
    echo "no RTL found under hardware/rtl - wrong directory?" >&2
    exit 1
fi

mkdir -p hardware/yosys
echo "=== yosys $(yosys -V 2>/dev/null | head -1) : synthesising soc_top for xc7 ==="
echo "    ${#FILES[@]} source files"

# -q keeps the log quiet; the stat output is what we want. program.hex/gpu_demo.hex
# are absent on a fresh clone - Yosys warns and leaves those memories empty, which
# is fine for an area estimate (nothing here is constant-folded because the images
# are inputs, not parameters).
yosys -q -p "
  $(printf 'read_verilog %s; ' "${FILES[@]}")
  hierarchy -top soc_top;
  synth_xilinx -family xc7 -top soc_top;
  stat;
  tee -o hardware/yosys/area.rpt stat
" || { echo "yosys failed - see the output above" >&2; exit 1; }

echo
echo "=== summary (Yosys' estimate, not Vivado's) ==="
grep -E "Number of (cells|wires)|\\\$_(DFF|LUT|SDFF|MUX|TBUF)" hardware/yosys/area.rpt 2>/dev/null | tail -20
echo
echo "full report: hardware/yosys/area.rpt"
echo
echo "Context: the Vivado 2026.1 run of 2026-08-20 reported 7,516 LUTs, 2,410 FF,"
echo "64.5 BRAM tiles for an EARLIER revision of this design (pre-GPU-load-port,"
echo "pre-unified-memory). A Yosys number that is the same order of magnitude is the"
echo "claim being checked: that this RTL still maps to hardware at all."
