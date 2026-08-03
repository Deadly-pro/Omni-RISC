// Omni-RISC APU — GPU Top-Level
//
// Integrates: gpu_cmd_proc, warp_scheduler, gpu_fetch, gpu_decode, 4x exec_lane.
// Each exec_lane is ONE WARP (4 SIMT sub-lanes) with its own regfile and
// 4-bank scratchpad. Single-cycle execution: regfile read, ALU/scratchpad
// access, and writeback all resolve in the issue cycle (everything but the
// register/scratchpad writes is combinational).
module gpu_top #(
    parameter IMEM_FILE = ""
) (
    input         clk,
    input         reset,

    // PBUS interface (from CPU/SoC)
    input  [31:0] pbus_addr,
    input  [31:0] pbus_wdata,
    input  [3:0]  pbus_wen,
    input         pbus_read,
    output [31:0] pbus_rdata,
    output        pbus_ready,

    // status
    output [3:0]  active_warps
);

    // ---- warp_scheduler <-> gpu_fetch ----
    wire [1:0]  fetch_warp_id;
    wire [31:0] fetch_pc;
    wire [15:0] fetch_instr;
    wire        fetch_valid;

    // ---- warp_scheduler <-> issue ----
    wire [1:0]  issue_warp_id;
    wire [15:0] issue_instr;
    wire        issue_valid;
    wire        issue_ready = 1'b1;   // single-cycle lanes, always ready

    // ---- exec completion (registered below) ----
    reg [1:0] complete_warp_id;
    reg       complete_valid;

    // ---- command processor ----
    wire [31:0] cmd_warp_pc;
    wire [1:0]  cmd_warp_id;
    wire        cmd_launch;

    gpu_cmd_proc u_cmd (
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(pbus_rdata),
        .pbus_ready(pbus_ready),
        .cmd_warp_pc(cmd_warp_pc),
        .cmd_warp_id(cmd_warp_id),
        .cmd_launch(cmd_launch),
        .active_warps(active_warps)
    );

    // ---- decode ----
    wire [3:0]  dec_alu_op;
    wire [2:0]  dec_rd_addr, dec_rs1_addr, dec_rs2_addr;
    wire        dec_reg_write, dec_mem_read, dec_mem_write;
    wire        dec_is_ldi, dec_is_halt, dec_is_branch;
    wire [31:0] dec_ldi_imm;
    wire [7:0]  dec_branch_target;

    gpu_decode u_dec (
        .instr(issue_instr),
        .alu_op(dec_alu_op),
        .rd_addr(dec_rd_addr),
        .rs1_addr(dec_rs1_addr),
        .rs2_addr(dec_rs2_addr),
        .reg_write(dec_reg_write),
        .mem_read(dec_mem_read),
        .mem_write(dec_mem_write),
        .is_ldi(dec_is_ldi),
        .ldi_imm(dec_ldi_imm),
        .is_halt(dec_is_halt),
        .is_branch(dec_is_branch),
        .branch_target(dec_branch_target)
    );

    // ---- warp scheduler ----
    warp_scheduler u_sched (
        .clk(clk),
        .reset(reset),
        .cmd_warp_pc(cmd_warp_pc),
        .cmd_warp_id(cmd_warp_id),
        .cmd_launch(cmd_launch),
        .fetch_warp_id(fetch_warp_id),
        .fetch_pc(fetch_pc),
        .fetch_instr(fetch_instr),
        .fetch_valid(fetch_valid),
        .issue_is_halt(dec_is_halt),
        .issue_is_branch(dec_is_branch),
        .issue_branch_target(dec_branch_target),
        .issue_warp_id(issue_warp_id),
        .issue_instr(issue_instr),
        .issue_valid(issue_valid),
        .issue_ready(issue_ready),
        .complete_warp_id(complete_warp_id),
        .complete_valid(complete_valid),
        .active_warps(active_warps),
        .debug_pc()
    );

    // instruction fetch
    gpu_fetch #(.IMEM_FILE(IMEM_FILE)) u_fetch (
        .clk(clk),
        .reset(reset),
        .warp_id(fetch_warp_id),
        .pc(fetch_pc),
        .instr(fetch_instr),
        .valid(fetch_valid)
    );

    // ---- 4 warps (exec_lane = one warp of 4 SIMT sub-lanes) ----
    // exec_lane's internal alu_op decode reads instr[3:0], so feed it the
    // pre-decoded alu_op in the low nibble.
    wire [15:0] lane_instr = {12'b0, dec_alu_op};

    genvar w;
    wire [127:0] alu_result [0:3];
    wire [127:0] ld_data    [0:3];
    wire [127:0] rs2_data   [0:3];
    wire [31:0]  lsu_addr   [0:3];
    wire [127:0] sp_rd_unused [0:3];

    generate
        for (w = 0; w < 4; w = w + 1) begin : g_warp
            wire sel = issue_valid && (issue_warp_id == w[1:0]);
            // writeback mux: load > ldi > alu
            wire [127:0] wb = dec_mem_read ? ld_data[w] :
                              dec_is_ldi   ? {4{dec_ldi_imm}} :
                              alu_result[w];
            exec_lane u_lane (
                .clk(clk), .reset(reset),
                .instr(lane_instr),
                .instr_valid(sel),
                .rs1_addr(dec_rs1_addr),
                .rs2_addr(dec_rs2_addr),
                .rd_addr(dec_rd_addr),
                .rd_write_en(sel && dec_reg_write),
                .rd_data(wb),
                .sp_write_en(sel && dec_mem_write),
                .sp_waddr(lsu_addr[w]),
                .sp_wdata(rs2_data[w]),
                .sp_raddr(lsu_addr[w]),
                .sp_rdata(sp_rd_unused[w]),
                .alu_result(alu_result[w]),
                .lsu_addr(lsu_addr[w]),
                .ld_data_out(ld_data[w]),
                .rs2_out(rs2_data[w])
            );
        end
    endgenerate

    // ---- completion: 1 cycle after issue ----
    always @(posedge clk) begin
        if (reset) begin
            complete_valid   <= 1'b0;
            complete_warp_id <= 2'b0;
        end else begin
            complete_valid   <= issue_valid && !dec_is_halt;
            complete_warp_id <= issue_warp_id;
        end
    end

endmodule
