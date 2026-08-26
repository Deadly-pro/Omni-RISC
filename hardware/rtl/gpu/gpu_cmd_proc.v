// Omni-RISC APU — GPU Command Processor
//
// Receives commands from CPU via PBUS, launches warps on GPU, and hosts the
// shared-memory window: the CPU writes kernel inputs into warp0's scratchpad
// and reads results back through HOST_DATA. Writes complete on the PBUS store
// before LAUNCH is issued (acquire for the GPU); the CPU polls active_warps to
// zero before reading results (release from the GPU).
module gpu_cmd_proc (
    input         clk,
    input         reset,
    // PBUS interface (from CPU)
    input  [31:0] pbus_addr,
    input  [31:0] pbus_wdata,
    input  [3:0]  pbus_wen,
    input         pbus_read,
    output [31:0] pbus_rdata,
    output        pbus_ready,
    // warp scheduler interface
    output [31:0] cmd_warp_pc,
    output [1:0]  cmd_warp_id,
    output        cmd_launch,
    // status
    input  [3:0]  active_warps,
    // completion interrupt: level-high when a warp just finished; cleared
    // on STATUS read (so the ISR reads STATUS, then clears msip = 0)
    output        gpu_done,
    // shared-memory window into warp0's scratchpad (128-bit = 4 lanes at one
    // word index: [31:0]=lane0, [63:32]=lane1, [95:64]=lane2, [127:96]=lane3)
    input  [127:0] host_rdata,
    output [7:0]  host_raddr,
    output [1:0]  host_wbank,
    output [7:0]  host_waddr,
    output reg [31:0] host_wdata,
    output reg        host_wen
);

    // Command registers (memory-mapped at 0x4000_2000)
    // 0x4000_2000: warp_pc[0]
    // 0x4000_2004: warp_pc[1]
    // 0x4000_2008: warp_pc[2]
    // 0x4000_200C: warp_pc[3]
    // 0x4000_2010: launch (bit[31] = go, bit[1:0] = warp_id)
    // 0x4000_2014: result (warp0 scratchpad word 0 readback)
    // 0x4000_2018: host_win ({bank[1:0], word[7:0]} for shared-memory access)
    // 0x4000_201C: host_data (write -> scratchpad store; read -> scratchpad load)
    // 0x4000_2020: status (unlisted -> returns active_warps)
    reg [31:0] warp_pc [0:3];
    reg        launch_pending;
    reg [1:0]  launch_warp_id;
    reg [9:0]  host_win;

    // done latch: set when active_warps goes 1->0, combinational-clear on read
    reg        gpu_done_latch;
    reg        was_busy;
    wire       gpu_done_clear = pbus_read && (pbus_addr[5:0] == 6'h20);

    always @(posedge clk) begin
        if (reset) begin
            gpu_done_latch <= 1'b0;
            was_busy       <= 1'b0;
        end else begin
            was_busy <= |active_warps;
            if (gpu_done_clear)
                gpu_done_latch <= 1'b0;
            else if (was_busy && !(|active_warps))
                gpu_done_latch <= 1'b1;
        end
    end
    // combinational so the ISR's read-STATUS clears the latch before the
    // write to msip (even though the register clears on the next edge)
    assign gpu_done = gpu_done_latch & ~gpu_done_clear;

    assign pbus_ready = 1'b1;

    always @(posedge clk) begin
        if (reset) begin
            warp_pc[0] <= 32'b0;
            warp_pc[1] <= 32'b0;
            warp_pc[2] <= 32'b0;
            warp_pc[3] <= 32'b0;
            launch_pending <= 1'b0;
            launch_warp_id <= 2'b0;
            host_win       <= 10'b0;
            host_wen       <= 1'b0;
            host_wdata     <= 32'b0;
        end else begin
            launch_pending <= 1'b0;
            host_wen       <= 1'b0;
            if (|pbus_wen && pbus_addr[31:5] == 27'h2000100) begin
                case (pbus_addr[4:0])
                    5'h00: warp_pc[0] <= pbus_wdata;
                    5'h04: warp_pc[1] <= pbus_wdata;
                    5'h08: warp_pc[2] <= pbus_wdata;
                    5'h0C: warp_pc[3] <= pbus_wdata;
                    5'h10: begin
                        if (pbus_wen[3]) begin
                            launch_pending <= pbus_wdata[31];  // go bit
                            launch_warp_id <= pbus_wdata[1:0]; // warp_id
                        end
                    end
                    5'h18: host_win <= pbus_wdata[9:0];   // bank + word select
                    5'h1C: begin
                        host_wdata <= pbus_wdata;
                        host_wen   <= 1'b1;               // store into scratchpad
                    end
                    default: ;
                endcase
            end
        end
    end

    assign cmd_warp_pc  = warp_pc[launch_warp_id];
    assign cmd_warp_id  = launch_warp_id;
    assign cmd_launch   = launch_pending;

    // shared-memory window outputs (warp0 scratchpad)
    assign host_raddr  = host_win[7:0];
    assign host_wbank  = host_win[9:8];
    assign host_waddr  = host_win[7:0];

    // readback
    wire [31:0] rdata_0 = warp_pc[0];
    wire [31:0] rdata_1 = warp_pc[1];
    wire [31:0] rdata_2 = warp_pc[2];
    wire [31:0] rdata_3 = warp_pc[3];
    wire [31:0] rdata_10 = {launch_pending, 29'b0, launch_warp_id};
    wire [31:0] rdata_14 = host_rdata[31:0];   // warp0 scratchpad word 0 readback
    wire [31:0] rdata_18 = {22'b0, host_win};
    wire [31:0] rdata_1C = host_rdata[host_win[9:8]*32 +: 32]; // lane word read
    wire [31:0] rdata_def = {28'b0, active_warps};

    assign pbus_rdata = (pbus_addr[31:6] != 26'h1000080) ? 32'b0 :
                        (pbus_addr[5:0] == 6'h00) ? rdata_0 :
                        (pbus_addr[5:0] == 6'h04) ? rdata_1 :
                        (pbus_addr[5:0] == 6'h08) ? rdata_2 :
                        (pbus_addr[5:0] == 6'h0C) ? rdata_3 :
                        (pbus_addr[5:0] == 6'h10) ? rdata_10 :
                        (pbus_addr[5:0] == 6'h14) ? rdata_14 :
                        (pbus_addr[5:0] == 6'h18) ? rdata_18 :
                        (pbus_addr[5:0] == 6'h1C) ? rdata_1C :
                        rdata_def;

endmodule
