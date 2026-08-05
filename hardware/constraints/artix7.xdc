## Omni-RISC APU — Artix-7 XC7A100T constraints
## Timing: a 50 MHz system clock (matches the fixed baud divider in uart.v,
## which divides 50 MHz by 434 to get 115200). Set the period here; physical
## pin constraints are board-specific and go below once a board is chosen.
create_clock -period 20.000 -name sys_clk [get_ports clk]

# --- Placeholder I/O assignment (fill in for a specific board) ---------------
# set_property PACKAGE_PIN <pin> [get_ports clk]
# set_property IOSTANDARD LVCMOS33 [get_ports clk]
# set_property PACKAGE_PIN <pin> [get_ports {reset}]
# set_property IOSTANDARD LVCMOS33 [get_ports {reset}]
# set_property PACKAGE_PIN <pin> [get_ports uart_tx]
# set_property IOSTANDARD LVCMOS33 [get_ports uart_tx]
