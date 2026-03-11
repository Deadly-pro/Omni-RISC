// Omni-RISC Simple Combinational-Read RAM for MVP
`timescale 1ns / 1ps

module simple_ram #(
    parameter MEM_SIZE = 16384, // 64KB (Adjust as needed)
    parameter INIT_FILE = "program.hex"
)(
    input  wire        clk,
    input  wire        rst,

    // Bus Slave Interface
    input  wire [31:0] addr,
    input  wire [31:0] wdata,
    input  wire        rd_en,
    input  wire        wr_en,
    output wire [31:0] rdata,
    output reg         ack
);

    reg [31:0] mem [0:MEM_SIZE-1];

    // Combinational Read
    assign rdata = (rd_en) ? mem[addr[31:2] % MEM_SIZE] : 32'b0;

    // Sequential Write and Ack generation
    always @(posedge clk) begin
        if (rst) begin
            ack   <= 1'b0;
        end else begin
            ack <= 0;
            if (wr_en) begin
                mem[addr[31:2] % MEM_SIZE] <= wdata;
                ack <= 1'b1;
            end else if (rd_en) begin
                ack <= 1'b1;
            end
        end
    end

    initial begin
        if (INIT_FILE != "") begin
            $display("Loading RAM from: %s", INIT_FILE);
            $readmemh(INIT_FILE, mem);
        end
    end

endmodule
