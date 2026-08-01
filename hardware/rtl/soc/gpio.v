// Omni-RISC APU — SoC: GPIO output
//
// Register (base 0x40001000):
//   +0x00 GPIO out — 8-bit write-only output. Reads return the current value.
module gpio (
    input         clk,
    input         reset,

    // ---- pbus slave port ----
    input  [31:0] pbus_addr,
    input  [31:0] pbus_wdata,
    input  [3:0]  pbus_wen,
    input         pbus_read,
    output [31:0] pbus_rdata,

    output reg [7:0] gpio_out
);
    wire sel = (pbus_addr[31:20] == 12'h400) && pbus_addr[12];   // 0x40001000

    always @(posedge clk) begin
        if (reset) gpio_out <= 8'b0;
        else if (sel && (pbus_wen != 0) && (pbus_addr[11:0] == 12'h000))
            gpio_out <= pbus_wdata[7:0];
    end

    reg [31:0] rdata;
    always @(*) begin
        rdata = sel ? {24'b0, gpio_out} : 32'b0;
    end
    assign pbus_rdata = rdata;
endmodule
