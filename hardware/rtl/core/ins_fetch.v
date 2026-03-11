// Simple Instruction Fetch stage for Omni-RISC (Combinational-Read Memory)
`timescale 1ns / 1ps

module ins_fetch (
    input  wire        clk,
    input  wire        rst,

    // Pipeline control
    input  wire        pipeline_stall_in,
    output wire        fetch_stall_out,

    // PC control
    input  wire [31:0] pc_in,
    output wire [31:0] pc_plus_4_out,

    // Instruction Bus Interface
    output wire [31:0] ibus_addr_out,
    output wire        ibus_req_out,
    input  wire [31:0] ibus_data_in,
    input  wire        ibus_ack_in,

    // Output to ID stage
    output wire [31:0] instruction_out,
    output wire [31:0] pc_out_pass
);

    assign pc_plus_4_out = pc_in + 32'd4;
    assign ibus_addr_out = pc_in;
    assign ibus_req_out  = !rst && !pipeline_stall_in;

    // With combinational read, data is valid if req and ack are both high in the same cycle.
    // However, our RAM ack is registered (it arrives in the cycle AFTER req).
    // So we MUST stall for one cycle whenever we make a request.
    assign fetch_stall_out = ibus_req_out && !ibus_ack_in;

    assign instruction_out = (ibus_ack_in) ? ibus_data_in : 32'h00000013;
    assign pc_out_pass     = pc_in;

endmodule

// Program counter register for a RISC-V core.
module prog_counter (
    input  wire        clk,
    input  wire        rst,
    input  wire [31:0] pc_in,
    output reg  [31:0] pc_out
);
    always @(posedge clk) begin
        if (rst) begin
            pc_out <= 32'h8000_0000; // Reset to start of RAM
        end else begin
            pc_out <= pc_in;
        end
    end
endmodule
