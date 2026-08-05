# impl.tcl — Vivado batch implementation (place + route) for the Omni-RISC APU
# Run after a successful `make synth`:
#   vivado -mode batch -source impl.tcl
#
# No board target: this is the physical-design gate. Opens the post-synthesis
# checkpoint (reports/synth.dcp), runs place + route, and reports timing
# closure (WNS / Fmax) + utilization on the nominal Artix-7 part. A bitstream
# is deliberately not written — it needs real pin constraints and a board.

set script_dir [file dirname [file normalize [info script]]]
file mkdir [file join $script_dir reports]

open_checkpoint [file join $script_dir reports synth.dcp]

place_design
route_design

report_utilization    -file [file join $script_dir reports impl_utilization.rpt]
report_timing_summary -file [file join $script_dir reports impl_timing.rpt]
report_timing -from [get_clocks sys_clk] -to [get_clocks sys_clk] \
    -file [file join $script_dir reports impl_timing_path.rpt]

write_checkpoint -force [file join $script_dir reports/impl.dcp]

puts "\n=== IMPLEMENTATION DONE ==="
puts "    Fmax       -> reports/impl_timing.rpt (WNS / period)"
puts "    utilization -> reports/impl_utilization.rpt"
