// Write-back stage integration for Omni-RISC
`timescale 1ns / 1ps

module ins_wb (
    input  wire        clk,
    input  wire        rst,
    input  wire [1:0]  ma_core_id_in,
    input  wire [31:0] alu_result_in,
    input  wire [31:0] read_data_in,
    input  wire [31:0] pc_plus_4_in,
    input  wire [4:0]  rd_addr_in,
    input  wire        reg_write_in,
    input  wire        mem_to_reg_in,
    input  wire        write_from_pc_in,

    output wire [31:0] wb_write_data_out,
    output wire [4:0]  wb_rd_addr_out,
    output wire        wb_reg_write_en_out
    );

    assign wb_write_data_out = (write_from_pc_in) ? pc_plus_4_in :
                               (mem_to_reg_in)    ? read_data_in :
                               alu_result_in;
    assign wb_rd_addr_out       = rd_addr_in;
    assign wb_reg_write_en_out  = reg_write_in;

endmodule

// MEM/WB stage pipeline register.
module mem_wb_buffer (
    input  wire        clk,
    input  wire        rst,
    input  wire        en,

    input  wire [1:0]  ma_core_id_in,
    input  wire [31:0] mem_alu_result_in,
    input  wire [31:0] mem_read_data_in,
    input  wire [4:0]  mem_rd_addr_in,
    input  wire [31:0] mem_pc_plus_4_in,
    input  wire        mem_reg_write_in,
    input  wire        mem_to_reg_in,      // Was missing
    input  wire        mem_write_from_pc_in,

    output reg  [31:0] wb_alu_result_out,
    output reg  [31:0] wb_read_data_out,
    output reg  [4:0]  wb_rd_addr_out,
    output reg  [31:0] wb_pc_plus_4_out,
    output reg         wb_reg_write_out,
    output reg         wb_mem_to_reg_out,
    output reg         wb_write_from_pc_out,
    output reg  [1:0]  wb_core_id_out
);

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            wb_alu_result_out   <= 32'b0;
            wb_read_data_out    <= 32'b0;
            wb_rd_addr_out      <= 5'b0;
            wb_pc_plus_4_out    <= 32'b0;
            wb_reg_write_out    <= 1'b0;
            wb_mem_to_reg_out   <= 1'b0;
            wb_write_from_pc_out <= 1'b0;
            wb_core_id_out      <= 2'b00;
        end else if (en) begin
            wb_alu_result_out   <= mem_alu_result_in;
            wb_read_data_out    <= mem_read_data_in;
            wb_rd_addr_out      <= mem_rd_addr_in;
            wb_pc_plus_4_out    <= mem_pc_plus_4_in;
            wb_reg_write_out    <= mem_reg_write_in;
            wb_mem_to_reg_out   <= mem_to_reg_in;
            wb_write_from_pc_out <= mem_write_from_pc_in;
            wb_core_id_out      <= ma_core_id_in;
        end
    end

endmodule
