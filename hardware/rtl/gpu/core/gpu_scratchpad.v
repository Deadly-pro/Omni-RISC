// Omni-RISC APU — GPU Core: gpu_scratchpad
//
// 4-lane SIMT scratchpad: 4 banks × 256 words × 32 bits (1KB per lane bank).
// Lane g accesses bank g. Packed 32-bit address bus: addr[7:0]=lane0,
// [15:8]=lane1, [23:16]=lane2, [31:24]=lane3. One read + one write port.
module gpu_scratchpad (
    input         clk,
    input         reset,

    // read port
    input  [31:0] raddr,        // {lane3..lane0} word addresses
    output [127:0] rdata,       // lane0..3 data

    // write port
    input  [31:0] waddr,
    input  [127:0] wdata,
    input         write_en
);
    reg [31:0] bank0 [0:255] /*verilator public*/;
    reg [31:0] bank1 [0:255] /*verilator public*/;
    reg [31:0] bank2 [0:255] /*verilator public*/;
    reg [31:0] bank3 [0:255] /*verilator public*/;
    integer i;

    always @(posedge clk) begin
        if (reset) begin
            for (i = 0; i < 256; i = i + 1) begin
                bank0[i] <= 32'b0; bank1[i] <= 32'b0;
                bank2[i] <= 32'b0; bank3[i] <= 32'b0;
            end
        end else if (write_en) begin
            bank0[waddr[7:0]]   <= wdata[31:0];
            bank1[waddr[15:8]]  <= wdata[63:32];
            bank2[waddr[23:16]] <= wdata[95:64];
            bank3[waddr[31:24]] <= wdata[127:96];
        end
    end

    assign rdata[31:0]   = bank0[raddr[7:0]];
    assign rdata[63:32]  = bank1[raddr[15:8]];
    assign rdata[95:64]  = bank2[raddr[23:16]];
    assign rdata[127:96] = bank3[raddr[31:24]];
endmodule
