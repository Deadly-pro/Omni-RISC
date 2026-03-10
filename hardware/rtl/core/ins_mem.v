// Refactored Memory stage logic for Omni-RISC
// Supports Data Bus (dbus) handshake and pipeline stalling
`timescale 1ns / 1ps

module ins_mem (
    input  wire        clk,
    input  wire        rst,
    
    // Pipeline control
    output wire        mem_stall_out,    // Stall caused by memory latency
    
    // Inputs from EX/MA buffer
    input  wire [31:0] alu_result_in,
    input  wire [31:0] rs2_data_in,
    input  wire [4:0]  rd_addr_in,
    input  wire [31:0] pc_plus_4_in,
    input  wire        mem_read_in,
    input  wire        mem_write_in,
    input  wire        reg_write_in,
    input  wire        mem_to_reg_in,
    input  wire        write_from_pc_in,

    // Data Bus Interface (Talks to L1 D-Cache or Bus Interconnect)
    output wire [31:0] dbus_addr_out,
    output wire [31:0] dbus_write_data_out,
    output wire        dbus_read_en_out,
    output wire        dbus_write_en_out,
    input  wire [31:0] dbus_read_data_in,
    input  wire        dbus_ack_in,       // High when data is valid or write is accepted

    // Outputs to MA/WB buffer
    output wire [31:0] alu_result_out,
    output wire [31:0] read_data_out,
    output wire [4:0]  rd_addr_out,
    output wire [31:0] pc_plus_4_out,
    output wire        reg_write_out,
    output wire        mem_to_reg_out,
    output wire        write_from_pc_out
);

    // Bus assignments
    assign dbus_addr_out       = alu_result_in;
    assign dbus_write_data_out = rs2_data_in;
    assign dbus_read_en_out    = mem_read_in;
    assign dbus_write_en_out   = mem_write_in;

    // Stall logic: If we are trying to read or write, we must wait for the bus ack
    assign mem_stall_out = (mem_read_in || mem_write_in) && !dbus_ack_in;

    // Pass-through or read data
    assign alu_result_out     = alu_result_in;
    assign read_data_out      = (mem_read_in && dbus_ack_in) ? dbus_read_data_in : 32'b0;
    assign rd_addr_out        = rd_addr_in;
    assign pc_plus_4_out      = pc_plus_4_in;
    assign reg_write_out      = reg_write_in;
    assign mem_to_reg_out     = mem_to_reg_in;
    assign write_from_pc_out  = write_from_pc_in;

endmodule
