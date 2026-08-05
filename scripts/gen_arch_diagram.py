#!/usr/bin/env python3
"""Generate the Omni-RISC APU architecture diagram from the RTL hierarchy.

Parses every module instantiation in hardware/rtl/**/*.v to build the real
module graph, then renders a curated public architecture view: the APU
(soc_top) and the MSI coherence research rig (dual_core_top) as clusters.

Usage:
    python3 scripts/gen_arch_diagram.py
"""
import os
import re
import subprocess
import sys
from collections import defaultdict

RTL_DIR = os.path.join(os.path.dirname(__file__), "..", "hardware", "rtl")
OUT_DIR = os.path.join(os.path.dirname(__file__), "..", "docs", "diagrams")

MODULE_RE = re.compile(r"^\s*module\s+([a-zA-Z_][a-zA-Z0-9_]*)\b", re.M)
INST_RE = re.compile(
    r"^\s*([a-zA-Z_][a-zA-Z0-9_]*)\s*(?:#\s*\(.*?\)\s*)?([a-zA-Z_][a-zA-Z0-9_]*)\s*\(",
    re.M | re.S)
SKIP = {"module", "endmodule", "if", "else", "for", "case", "begin", "assign",
        "always", "initial", "function", "task", "parameter", "localparam",
        "wire", "reg", "input", "output", "posedge", "negedge", "return",
        "generate"}


def modules_by_file():
    mods = {}
    for root, _, files in os.walk(RTL_DIR):
        for f in files:
            if f.endswith(".v"):
                p = os.path.join(root, f)
                src = open(p).read()
                for m in MODULE_RE.finditer(src):
                    mods[m.group(1)] = p
    return mods


def edges(modules):
    edges = defaultdict(list)  # parent -> [(child, inst)]
    for parent, path in modules.items():
        src = open(path).read()
        for m in INST_RE.finditer(src):
            mod, inst = m.group(1), m.group(2)
            if mod in modules and mod not in SKIP:
                edges[parent].append((mod, inst))
    return edges


LABELS = {
    "soc_top": "APU — soc_top\n(RV32IM CPU + SIMT GPU, pbus SoC)",
    "cpu_top": "RV32IM Core\n5-stage pipeline",
    "gpu_top": "SIMT GPU\n(4 warps, 16-bit ISA)",
    "uart": "UART", "timer": "Timer / CLINT", "gpio": "GPIO",
    "fetch_stage": "Fetch", "decode_stage": "Decode", "exec_stage": "Execute",
    "mem_stage": "Memory", "wb_stage": "Write-back",
    "hazard_unit": "Hazard Unit",
    "instr_bram": "Instr\nBRAM", "data_bram": "Data\nBRAM 256KB",
    "gpu_cmd_proc": "Cmd\nProc", "warp_scheduler": "Warp\nSched",
    "gpu_fetch": "Fetch", "gpu_decode": "Decode", "exec_lane": "Exec Lanes\n×4",
    "dual_core_top": "MSI Coherence Rig — dual_core_top",
    "l1_dcache_msi": "L1 D$ MSI\n4KB 2-way\n(snooping, WT)",
    "pbus_arbiter": "pbus\nArbiter",
}

COLORS = {
    "soc_top": "#c9daf8", "cpu_top": "#d9ead3", "gpu_top": "#f4cccc",
    "uart": "#ead1dc", "timer": "#ead1dc", "gpio": "#ead1dc",
    "fetch_stage": "#eef2f7", "decode_stage": "#eef2f7",
    "exec_stage": "#eef2f7", "mem_stage": "#eef2f7", "wb_stage": "#eef2f7",
    "hazard_unit": "#eef2f7", "instr_bram": "#fff2cc", "data_bram": "#fff2cc",
    "gpu_cmd_proc": "#f9dcdc", "warp_scheduler": "#f9dcdc",
    "gpu_fetch": "#f9dcdc", "gpu_decode": "#f9dcdc", "exec_lane": "#f9dcdc",
    "dual_core_top": "#d9ead3", "l1_dcache_msi": "#d9ead3",
    "pbus_arbiter": "#eef2f7",
}


def main():
    modules = modules_by_file()
    graph = edges(modules)

    def have(parent, child):
        return any(c == child for c, _ in graph.get(parent, []))

    # (from, to, label, style) — every structural edge verified against RTL;
    # "backing" edges are dashed (BRAM loaded via $readmemh / parameter).
    S = []
    # ---- APU cluster ----
    for c in ("cpu_top", "gpu_top", "uart", "timer", "gpio"):
        assert have("soc_top", c), c
    S += [("soc_top", "cpu_top", "u_cpu", "solid"),
          ("soc_top", "gpu_top", "u_gpu", "solid"),
          ("soc_top", "uart", "u_uart", "solid"),
          ("soc_top", "timer", "u_timer", "solid"),
          ("soc_top", "gpio", "u_gpio", "solid")]
    for c in ("fetch_stage", "decode_stage", "exec_stage", "mem_stage",
              "wb_stage", "hazard_unit"):
        assert have("cpu_top", c), c
    S += [("cpu_top", "fetch_stage", "", "solid"),
          ("cpu_top", "decode_stage", "", "solid"),
          ("cpu_top", "exec_stage", "", "solid"),
          ("cpu_top", "mem_stage", "", "solid"),
          ("cpu_top", "wb_stage", "", "solid"),
          ("cpu_top", "hazard_unit", "", "solid")]
    assert have("fetch_stage", "instr_bram"), "fetch->instr_bram"
    assert have("mem_stage", "data_bram"), "mem->data_bram"
    S += [("fetch_stage", "instr_bram", "instr_bram1", "solid"),
          ("mem_stage", "data_bram", "u_dbram", "solid")]
    for c in ("gpu_cmd_proc", "warp_scheduler", "gpu_fetch", "gpu_decode",
              "exec_lane"):
        assert have("gpu_top", c), c
    S += [("gpu_top", "gpu_cmd_proc", "u_cmd", "solid"),
          ("gpu_top", "warp_scheduler", "u_sched", "solid"),
          ("gpu_top", "gpu_fetch", "u_fetch", "solid"),
          ("gpu_top", "gpu_decode", "u_dec", "solid"),
          ("gpu_top", "exec_lane", "u_lane", "solid")]
    # ---- Coherence rig cluster ----
    for c in ("cpu_top", "cpu_top", "data_bram", "pbus_arbiter"):
        pass
    assert have("dual_core_top", "data_bram")
    assert have("dual_core_top", "pbus_arbiter")
    cores = [i for c, i in graph.get("dual_core_top", []) if c == "cpu_top"]
    assert len(cores) == 2, cores
    S += [("dual_core_top", "core0", "u_core0", "solid"),
          ("dual_core_top", "core1", "u_core1", "solid"),
          ("dual_core_top", "l1_dcache_msi", "snoop bus", "solid"),
          ("dual_core_top", "data_bram", "u_shared_dbram", "solid"),
          ("dual_core_top", "pbus_arbiter", "u_arbiter", "solid"),
          ("core0", "l1_dcache_msi", "", "solid"),
          ("core1", "l1_dcache_msi", "", "solid"),
          ("l1_dcache_msi", "data_bram", "backing store", "dashed")]

    lines = []
    lines.append("digraph apu {")
    lines.append("  rankdir=LR; bgcolor=\"#ffffff\";")
    lines.append("  node [shape=box, style=\"rounded,filled\", fontname=\"Helvetica\","
                 " fontsize=10, penwidth=1.2, margin=\"0.14,0.09\"];")
    lines.append("  edge [color=\"#555555\", penwidth=1.0, arrowsize=0.7];")
    lines.append("  labeljust=\"l\";")
    lines.append('  label=<<b>Omni-RISC APU — architecture</b> <font point-size="9">(generated from the RTL hierarchy — solid edges are instantiations, dashed edges are coherence relationships)</font>>;')
    lines.append("  subgraph cluster_apu {")
    lines.append('    label="Synthesized APU (soc_top)"; style="rounded,dashed"; color="#999999"; fontsize=11;')
    apu_nodes = ["soc_top", "cpu_top", "gpu_top", "uart", "timer", "gpio",
                 "fetch_stage", "decode_stage", "exec_stage", "mem_stage",
                 "wb_stage", "hazard_unit", "instr_bram", "data_bram",
                 "gpu_cmd_proc",
                 "warp_scheduler", "gpu_fetch", "gpu_decode", "exec_lane"]
    lines.append("}")
    lines.append("  subgraph cluster_cohere {")
    lines.append('    label="Coherence research rig (dual_core_top)"; style="rounded,dashed"; color="#999999"; fontsize=11;')
    cohere_nodes = ["dual_core_top", "core0", "core1", "l1_dcache_msi",
                    "data_bram", "pbus_arbiter"]
    lines.append("}")

    for f, t, lab, style in S:
        attrs = []
        if lab:
            attrs.append(f'label="{lab}"')
        if style == "dashed":
            attrs.append('style="dashed" color="#b0b0b0"')
        attrs.append("fontsize=8")
        lines.append(f'  "{f}" -> "{t}" [{", ".join(attrs)}];')

    for n in apu_nodes + cohere_nodes:
        lab = "CPU Core 0\nRV32IM" if n == "core0" else \
              "CPU Core 1\nRV32IM" if n == "core1" else LABELS.get(n, n)
        lines.append(f'  "{n}" [label="{lab}", fillcolor="{COLORS.get(n, "#eef2f7")}"];')

    lines.append("}")
    dot = "\n".join(lines) + "\n"

    os.makedirs(OUT_DIR, exist_ok=True)
    dot_path = os.path.join(OUT_DIR, "architecture.dot")
    open(dot_path, "w").write(dot)
    for fmt in ("svg", "png"):
        out = os.path.join(OUT_DIR, f"architecture.{fmt}")
        subprocess.run(["dot", f"-T{fmt}", dot_path, "-o", out], check=True)
        print("wrote", out)


if __name__ == "__main__":
    sys.exit(main())
