// Omni-RISC Simple Debug UART (Write-Only for MVP)
`timescale 1ns / 1ps

module simple_uart (
    input  wire        clk,
    input  wire        rst,

    // Bus Slave Interface
    input  wire [31:0] addr,
    input  wire [31:0] wdata,
    input  wire        rd_en,
    input  wire        wr_en,
    output reg  [31:0] rdata,
    output reg         ack,

    // Physical Serial Interface
    output reg         tx
);

    // For simulation/FPGA, we'll just use a task or a simple shift register.
    // In a "barely booting" MVP, we can even just use $display in simulation.

    reg tx_done;

    always @(posedge clk or posedge rst) begin
        if (rst) begin
            ack   <= 1'b0;
            rdata <= 32'b0;
            tx    <= 1'b1; // Idle high
            tx_done <= 1'b0;
        end else begin
            if (wr_en) begin
                ack <= 1'b1;
                if (!tx_done) begin
                    if (addr[7:0] == 8'h00) begin
                        $write("%c", wdata[7:0]);
                        $fflush();
                    end
                    tx_done <= 1'b1;
                end
            end else if (rd_en) begin
                ack <= 1'b1;
                if (addr[7:0] == 8'h04) begin
                    rdata <= 32'h00000001;
                end else begin
                    rdata <= 32'b0;
                end
            end else begin
                ack <= 0;
                tx_done <= 0;
            end
        end
    end

endmodule
