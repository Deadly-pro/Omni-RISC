# synth.tcl — Vivado batch synthesis for the Omni-RISC APU
# Run from hardware/vivado (via `make synth`):  vivado -mode batch -source synth.tcl
#
# Synthesizes the full SoC (soc_top: RV32IM CPU + UART/timer/GPIO + SIMT GPU)
# down to a netlist for the Artix-7 XC7A100T, then writes utilization and
# timing reports. Implementation (place+route) is a separate step: impl.tcl.
#
# Usage overrides:  vivado -mode batch -source synth.tcl -tclargs xc7a100tcsg324-1 soc_top

set part [lindex $argv 0]
if {$part eq ""} { set part "xc7a100tcsg324-1" }

set top [lindex $argv 1]
if {$top eq ""} { set top "soc_top" }

set script_dir [file dirname [file normalize [info script]]]
set rtl_dir    [file join $script_dir .. rtl]
set xdc_file   [file join $script_dir .. constraints artix7.xdc]
file mkdir     [file join $script_dir reports]

# --- Recursively collect every .v under hardware/rtl -------------------------
proc collect_rtl {dir} {
    set out {}
    foreach f [glob -nocomplain -directory $dir *.v] { lappend out $f }
    foreach d [glob -nocomplain -directory $dir -type d *] {
        set out [concat $out [collect_rtl $d]]
    }
    return $out
}
set rtl_files [collect_rtl $rtl_dir]
puts "RTL files: [llength $rtl_files]"

# --- The RTL loads firmware via $readmemh at elaboration; the image must
# --- exist on disk AND be non-trivial, or the memories constant-fold and the
# --- whole design collapses (0 LUTs/FFs). `make synth` builds these first;
# --- here we stage the real images under the names the RTL reads.
set project_root [file join $script_dir .. ..]
foreach pair [list \
        [list [file join $project_root firmware benchmark_gpu.hex] program.hex] \
        [list [file join $project_root firmware gpu_kernels gpu_demo.hex] gpu_demo.hex]] {
    set src [lindex $pair 0]
    set dst [file join $script_dir [lindex $pair 1]]
    if {[file exists $src]} {
        file copy -force $src $dst
    } else {
        puts "WARNING: $src not found — synthesizing with empty BRAM init; design may constant-fold"
        set fh [open $dst w]; close $fh
    }
}

create_project -in_memory -part $part

foreach f $rtl_files { read_verilog $f }
read_xdc $xdc_file

synth_design -top $top

report_utilization   -file [file join $script_dir reports synth_utilization.rpt]
report_timing_summary -file [file join $script_dir reports synth_timing.rpt]
write_checkpoint -force [file join $script_dir reports synth.dcp]

puts "\n=== SYNTHESIS DONE: $top on $part ==="
puts "    utilization -> reports/synth_utilization.rpt"
puts "    timing      -> reports/synth_timing.rpt"
puts "    netlist     -> reports/synth.dcp"
