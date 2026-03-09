/*
 * soc_top.v - Omni-RISC Top-Level System-on-Chip
 * 
 * This module connects the CPU, the MMU, the RAM, and Peripherals
 * (UART/CLINT) through the Bus Interconnect.
 */

module soc_top (
    input wire clk,
    input wire rst_n,
    
    // External interfaces (e.g., to FPGA Pins)
    input  wire uart_rx,
    output wire uart_tx
);

    // Internal Bus signals
    // Add your bus signals (e.g., AXI, Wishbone, or TileLink) here

    // 1. CPU Core (RV32IMACV)
    cpu_core u_cpu (
        .clk(clk),
        .rst_n(rst_n)
        // Add signals for Memory, Interrupts, etc.
    );

    // 2. Bus Interconnect
    // Maps 0x80000000 -> RAM
    // Maps 0x10000000 -> UART
    // Maps 0x02000000 -> CLINT

endmodule
