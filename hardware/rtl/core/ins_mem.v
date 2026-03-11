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

module ex_ma_buffer (
    input  wire        clk,
    input  wire        rst,
    input  wire        en,

    input  wire [31:0] ex_pc_plus_4_in,
    input  wire [31:0] ex_alu_result_in,
    input  wire [31:0] ex_read_data2_in,
    input  wire [4:0]  ex_rd_addr_in,
    input  wire        ex_mem_read_in,
    input  wire        ex_mem_write_in,
    input  wire        ex_reg_write_in,
    input  wire        ex_mem_to_reg_in,
    input  wire        ex_branch_in,
    input  wire        ex_write_from_pc_in,

    output reg  [31:0] ma_pc_plus_4_out,
    output reg  [31:0] ma_alu_result_out,
    output reg  [31:0] ma_write_data_out,
    output reg  [4:0]  ma_rd_addr_out,
    output reg         ma_mem_read_out,
    output reg         ma_mem_write_out,
    output reg         ma_reg_write_out,
    output reg         ma_mem_to_reg_out,
    output reg         ma_write_from_pc_out
);

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            ma_pc_plus_4_out    <= 32'b0;
            ma_alu_result_out   <= 32'b0;
            ma_write_data_out   <= 32'b0;
            ma_rd_addr_out      <= 5'b0;
            ma_mem_read_out     <= 1'b0;
            ma_mem_write_out    <= 1'b0;
            ma_reg_write_out    <= 1'b0;
            ma_mem_to_reg_out   <= 1'b0;
            ma_write_from_pc_out <= 1'b0;
        end else if (en) begin
            ma_pc_plus_4_out    <= ex_pc_plus_4_in;
            ma_alu_result_out   <= ex_alu_result_in;
            ma_write_data_out   <= ex_read_data2_in;
            ma_rd_addr_out      <= ex_rd_addr_in;
            ma_mem_read_out     <= ex_mem_read_in;
            ma_mem_write_out    <= ex_mem_write_in;
            ma_reg_write_out    <= ex_reg_write_in;
            ma_mem_to_reg_out   <= ex_mem_to_reg_in;
            ma_write_from_pc_out <= ex_write_from_pc_in;
        end
    end

endmodule
