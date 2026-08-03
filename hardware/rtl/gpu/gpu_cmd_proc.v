// Omni-RISC APU — GPU Command Processor
//
// Receives commands from CPU via PBUS, launches warps on GPU.
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
    input  [3:0]  active_warps
);

    // Command registers (memory-mapped at 0x4000_2000)
    // 0x4000_2000: warp_pc[0]
    // 0x4000_2004: warp_pc[1]
    // 0x4000_2008: warp_pc[2]
    // 0x4000_200C: warp_pc[3]
    // 0x4000_2010: launch (bit[31] = go, bit[1:0] = warp_id)
    reg [31:0] warp_pc [0:3];
    reg        launch_pending;
    reg [1:0]  launch_warp_id;

    assign pbus_ready = 1'b1;

    always @(posedge clk) begin
        if (reset) begin
            warp_pc[0] <= 32'b0;
            warp_pc[1] <= 32'b0;
            warp_pc[2] <= 32'b0;
            warp_pc[3] <= 32'b0;
            launch_pending <= 1'b0;
            launch_warp_id <= 2'b0;
        end else begin
            launch_pending <= 1'b0;
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
                    default: ;
                endcase
            end
        end
    end

    assign cmd_warp_pc  = warp_pc[launch_warp_id];
    assign cmd_warp_id  = launch_warp_id;
    assign cmd_launch   = launch_pending;

    // readback
    wire [31:0] rdata_0 = warp_pc[0];
    wire [31:0] rdata_1 = warp_pc[1];
    wire [31:0] rdata_2 = warp_pc[2];
    wire [31:0] rdata_3 = warp_pc[3];
    wire [31:0] rdata_10 = {launch_pending, 29'b0, launch_warp_id};
    wire [31:0] rdata_def = {28'b0, active_warps};

    assign pbus_rdata = (pbus_addr[4:0] == 5'h00) ? rdata_0 :
                        (pbus_addr[4:0] == 5'h04) ? rdata_1 :
                        (pbus_addr[4:0] == 5'h08) ? rdata_2 :
                        (pbus_addr[4:0] == 5'h0C) ? rdata_3 :
                        (pbus_addr[4:0] == 5'h10) ? rdata_10 :
                        rdata_def;

endmodule