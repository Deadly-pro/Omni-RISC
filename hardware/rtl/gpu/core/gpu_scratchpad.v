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
    input         write_en,

    // host window — lets the CPU/SoC access scratchpad memory directly.
    // Read: one word index across all 4 banks (host_rdata[31:0]=bank0,
    // [63:32]=bank1, ...). Combinational.
    input  [7:0]  host_raddr,
    output [127:0] host_rdata,
    // Write: single 32-bit word into one bank (coarse-grained shared memory —
    // CPU writes kernel inputs before launch, reads results after halt).
    input  [1:0]  host_wbank,
    input  [7:0]  host_waddr,
    input  [31:0] host_wdata,
    input         host_wen
);
    reg [31:0] bank0 [0:255] /*verilator public*/;
    reg [31:0] bank1 [0:255] /*verilator public*/;
    reg [31:0] bank2 [0:255] /*verilator public*/;
    reg [31:0] bank3 [0:255] /*verilator public*/;

    // One write port per bank (distributed-RAM inferable): the lane write port
    // is shared with the host window — host takes priority, and the coherence
    // protocol guarantees the two never legitimately collide (host writes
    // inputs before launch, reads results after halt). No reset: BRAM/LUTRAM
    // cannot be reset, and write-before-read is guaranteed by the protocol.

    always @(posedge clk) begin
        if (host_wen && host_wbank == 2'd0)
            bank0[host_waddr] <= host_wdata;
        else if (write_en)
            bank0[waddr[7:0]] <= wdata[31:0];
    end

    always @(posedge clk) begin
        if (host_wen && host_wbank == 2'd1)
            bank1[host_waddr] <= host_wdata;
        else if (write_en)
            bank1[waddr[15:8]] <= wdata[63:32];
    end

    always @(posedge clk) begin
        if (host_wen && host_wbank == 2'd2)
            bank2[host_waddr] <= host_wdata;
        else if (write_en)
            bank2[waddr[23:16]] <= wdata[95:64];
    end

    always @(posedge clk) begin
        if (host_wen && host_wbank == 2'd3)
            bank3[host_waddr] <= host_wdata;
        else if (write_en)
            bank3[waddr[31:24]] <= wdata[127:96];
    end

    assign rdata[31:0]   = bank0[raddr[7:0]];
    assign rdata[63:32]  = bank1[raddr[15:8]];
    assign rdata[95:64]  = bank2[raddr[23:16]];
    assign rdata[127:96] = bank3[raddr[31:24]];

    assign host_rdata[31:0]   = bank0[host_raddr];
    assign host_rdata[63:32]  = bank1[host_raddr];
    assign host_rdata[95:64]  = bank2[host_raddr];
    assign host_rdata[127:96] = bank3[host_raddr];
endmodule
