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
    output [3:0]  active_warps,
    output        gpu_done,      // completion interrupt (see gpu_cmd_proc)

    // ---- global memory master port (M2) ------------------------------------
    // To soc_top's mem_arbiter: LDG/STG reach the same data_bram the CPU uses, so
    // a kernel works on the CPU's own arrays with no MMIO staging.
    output [31:0] mem_addr,
    output [31:0] mem_wdata,
    output [3:0]  mem_wen,
    output        mem_read_req,
    output        mem_write_req,
    input  [31:0] mem_rdata,
    input         mem_read_ack,
    input         mem_write_ack
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
    // Lanes are single-cycle, so issue is normally always ready. A global memory
    // access (LDG/STG) needs the bus, so acceptance of such an instruction waits
    // for its transaction - see the g_* block below.
    wire        issue_ready;

    // ---- exec completion (registered below) ----
    reg [1:0] complete_warp_id;
    reg       complete_valid;

    // ---- command processor ----
    wire [31:0] cmd_warp_pc;
    wire [1:0]  cmd_warp_id;
    wire        cmd_launch;
    wire [31:0] mem_base;      // GPU global-memory base (GPU_MEM_BASE, +0x2C)
    // shared-memory window: CPU writes kernel inputs / reads results in
    // warp0's scratchpad through gpu_cmd_proc host registers.
    wire [7:0]  host_raddr;
    wire [1:0]  host_wbank;
    wire [7:0]  host_waddr;
    wire [31:0] host_wdata;
    wire        host_wen;
    wire [127:0] host_rdata0;

    // kernel load port: gpu_cmd_proc -> gpu_fetch imem
    wire [9:0]  imem_addr;
    wire [15:0] imem_data;
    wire        imem_we;

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
        .active_warps(active_warps),
        .gpu_done(gpu_done),
        .host_rdata(host_rdata0),
        .host_raddr(host_raddr),
        .host_wbank(host_wbank),
        .host_waddr(host_waddr),
        .host_wdata(host_wdata),
        .host_wen(host_wen),
        .imem_we(imem_we),
        .imem_addr(imem_addr),
        .imem_data(imem_data),
        .mem_base(mem_base),
        .dbg_starts(dbg_starts),
        .dbg_state(dbg_state),
        .dbg_last(dbg_last)
    );

    // ---- decode ----
    wire [3:0]  dec_alu_op;
    wire [2:0]  dec_rd_addr, dec_rs1_addr, dec_rs2_addr;
    wire        dec_reg_write, dec_mem_read, dec_mem_write;
    wire        dec_is_ldi, dec_is_halt, dec_is_branch, dec_is_barrier;
    wire        dec_is_gld, dec_is_gst;
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
        .is_barrier(dec_is_barrier),
        .is_gld(dec_is_gld),
        .is_gst(dec_is_gst),
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
        .issue_is_barrier(dec_is_barrier),
        .issue_branch_target(dec_branch_target),
        .issue_warp_id(issue_warp_id),
        .issue_instr(issue_instr),
        .issue_valid(issue_valid),
        .issue_ready(issue_ready),
        .complete_warp_id(complete_warp_id),
        .complete_valid(complete_valid),
        .active_warps(active_warps)
    );

    // instruction fetch
    gpu_fetch #(.IMEM_FILE(IMEM_FILE)) u_fetch (
        .clk(clk),
        .reset(reset),
        .imem_we(imem_we),
        .imem_addr(imem_addr),
        .imem_data(imem_data),
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
    wire [127:0] host_rdata [0:3];
    wire [31:0]  g_addr [0:3];        // per-warp lane-0 global address

    generate
        for (w = 0; w < 4; w = w + 1) begin : g_warp
            // Acceptance, not mere presence: while a global access is in flight
            // the lanes must not act, or a pending load writes back stale data
            // and a store fires repeatedly.
            wire sel = issue_valid && issue_ready && (issue_warp_id == w[1:0]);
            // writeback mux: global load > scratchpad load > ldi > alu
            wire [127:0] wb = dec_is_gld   ? {4{g_rdata}} :
                              dec_mem_read ? ld_data[w] :
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
                // shared-memory window lives in warp0's scratchpad
                .host_raddr(host_raddr),
                .host_rdata(host_rdata[w]),
                .host_wbank(host_wbank),
                .host_waddr(host_waddr),
                .host_wdata(host_wdata),
                .host_wen(w == 0 ? host_wen : 1'b0),
                .alu_result(alu_result[w]),
                .lsu_addr(lsu_addr[w]),
                .ld_data_out(ld_data[w]),
                .g_addr(g_addr[w]),
                .rs2_out(rs2_data[w])
            );
        end
    endgenerate

    assign host_rdata0 = host_rdata[0];

    // ---- global memory (LDG/STG): one warp-uniform word per instruction -------
    // The transaction starts as soon as the instruction is presented, and the
    // instruction is accepted only once its data has arrived, so the lanes'
    // writeback lands on the acceptance cycle like every other instruction - and
    // `sel` below is gated by issue_ready so no lane acts while the bus is busy.
    // A store is one full word: all four lanes hold the same address and value.
    wire        is_global  = dec_is_gld | dec_is_gst;
    wire [31:0] g_sel_addr = g_addr[issue_warp_id];
    wire        g_start    = issue_valid & ~g_busy & ~g_done & is_global;
    wire        g_finish   = g_busy & (g_is_load ? mem_read_ack : mem_write_ack);

    reg         g_busy, g_done, g_is_load;
    reg  [31:0] g_rdata;
    // The request must be HELD, not recomputed: the transaction outlives the
    // cycle that issued it, and by then the decode/issue stage belongs to another
    // warp - taking the address combinationally off g_addr[] gave the store
    // whatever address that warp happened to hold (found via dbg_last: a store to
    // base+0 instead of base+8).
    reg  [31:0] g_addr_l, g_wdata_l;

    always @(posedge clk) begin
        if (reset) begin
            g_busy <= 1'b0; g_done <= 1'b0; g_is_load <= 1'b0; g_rdata <= 32'b0;
            g_addr_l <= 32'b0; g_wdata_l <= 32'b0;
        end else begin
            if (g_start) begin
                g_busy    <= 1'b1;
                g_is_load <= dec_is_gld;
                g_done    <= 1'b0;   // never let a stale flag accept the next one
                g_addr_l  <= mem_base + g_sel_addr;
                g_wdata_l <= rs2_data[issue_warp_id][31:0];
            end
            if (g_finish) begin
                g_busy <= 1'b0;
                g_done <= 1'b1;
                if (g_is_load) g_rdata <= mem_rdata;
            end
            // accepted: the instruction leaves the issue stage, so consume the flag
            if (issue_valid & issue_ready & is_global) g_done <= 1'b0;
        end
    end

    assign issue_ready   = is_global ? g_done : 1'b1;
    assign mem_addr      = g_addr_l;                        // latched at g_start
    assign mem_wdata     = g_wdata_l;                       // latched, uniform: lane 0's
    assign mem_wen       = (g_busy & ~g_is_load) ? 4'hF : 4'b0;
    assign mem_read_req  = g_busy &  g_is_load;
    assign mem_write_req = g_busy & ~g_is_load;

    // Debug visibility for the handshake (readable at GPU +0x30/+0x34 via
    // gpu_cmd_proc). A global access that never issues and one that issues but
    // never completes look identical from the outside otherwise.
    reg [31:0] dbg_starts, dbg_state, dbg_last;
    always @(posedge clk) begin
        if (reset) begin
            dbg_starts <= 32'b0;
            dbg_state  <= 32'b0;
            dbg_last   <= 32'b0;
        end else begin
            if (g_start) begin
                dbg_starts <= dbg_starts + 32'b1;
            end
            if (g_finish)
                dbg_last <= {g_is_load, (mem_wen != 4'b0), 2'b00, mem_addr[27:0]};
            dbg_state  <= {27'b0, g_is_load, mem_base[0], g_busy, g_done,
                           (mem_wen != 4'b0)};
        end
    end

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
