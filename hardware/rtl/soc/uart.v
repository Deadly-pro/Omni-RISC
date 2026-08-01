// Omni-RISC APU — SoC: UART transmitter (memory-mapped)
//
// Registers (base 0x40000000):
//   +0x00 TX      write a byte to transmit (8N1 @115200)
//   +0x04 STATUS  bit0 = TX busy (1 = a byte is being shifted out)
//
// Each pbus slave returns 0 on pbus_rdata when not selected, so soc_top can
// OR the slave read busses together.
module uart (
    input         clk,
    input         reset,

    // ---- pbus slave port ----
    input  [31:0] pbus_addr,
    input  [31:0] pbus_wdata,
    input  [3:0]  pbus_wen,
    input         pbus_read,
    output [31:0] pbus_rdata,

    output reg    uart_tx
);
    localparam CLKS_PER_BIT = 434;      // 50 MHz / 115200 baud
    wire sel = (pbus_addr[31:20] == 12'h400) && ~pbus_addr[12];   // 0x40000000

    reg tx_busy;
    reg [7:0] tx_shift;
    reg [3:0] bit_cnt;                  // 0=start, 1..8=data, 9=stop
    reg [9:0] clk_cnt;                  // bit-period counter

    wire write_tx = sel && (pbus_wen != 0) && (pbus_addr[11:0] == 12'h000);

    always @(posedge clk) begin
        if (reset) begin
            tx_busy  <= 1'b0;
            tx_shift <= 8'b0;
            bit_cnt  <= 4'b0;
            clk_cnt  <= 10'b0;
        end else if (write_tx) begin
            tx_shift <= pbus_wdata[7:0];
            bit_cnt  <= 4'b0;
            clk_cnt  <= 10'b0;
            tx_busy  <= 1'b1;
        end else if (tx_busy) begin
            if (clk_cnt == CLKS_PER_BIT - 1) begin
                clk_cnt <= 10'b0;
                if (bit_cnt == 4'd9) tx_busy <= 1'b0;   // stop bit done
                else                bit_cnt <= bit_cnt + 1'b1;
            end else begin
                clk_cnt <= clk_cnt + 1'b1;
            end
        end
    end

    // 8N1: start=0, data LSB-first, stop=1
    always @(*) begin
        if (!tx_busy) uart_tx = 1'b1;
        else case (bit_cnt)
            4'd0:                    uart_tx = 1'b0;
            4'd1,4'd2,4'd3,4'd4,
            4'd5,4'd6,4'd7,4'd8:     uart_tx = tx_shift[bit_cnt-1];
            default:                 uart_tx = 1'b1;
        endcase
    end

    // read path: STATUS[0] = TX busy
    reg [31:0] rdata;
    always @(*) begin
        rdata = 32'b0;
        if (sel && (pbus_addr[11:0] == 12'h004)) rdata = {31'b0, tx_busy};
    end
    assign pbus_rdata = rdata;
endmodule
