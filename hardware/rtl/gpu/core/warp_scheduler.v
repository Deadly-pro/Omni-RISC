// Omni-RISC APU — GPU Core: warp_scheduler
//
// Manages multiple warps (groups of 4 lanes), issues instructions from ready
// warps. Round-robin fetch rotates over all warp slots every cycle; issue
// follows fetch by one cycle (gpu_fetch's imem read is registered), so the
// instruction issuing for warp w is exactly the one fetched at pc[w] last
// cycle. Warps: idle -> ready (launch) -> executing (issue) -> ready
// (complete) -> ... -> idle (HALT).
module warp_scheduler #(
    parameter NUM_WARPS = 4
) (
    input         clk,
    input         reset,

    // command processor interface
    input  [31:0] cmd_warp_pc,     // PC to set for a warp
    input  [1:0]  cmd_warp_id,     // warp to configure
    input         cmd_launch,      // launch warp

    // fetch interface
    output [1:0]  fetch_warp_id,   // warp to fetch for
    output [31:0] fetch_pc,        // PC for fetch
    input  [15:0] fetch_instr,     // fetched instruction (1-cycle latency)
    input         fetch_valid,     // fetch returned valid instruction

    // decode results for the issuing instruction (from gpu_decode)
    input         issue_is_halt,   // HALT: warp goes idle
    input         issue_is_branch, // unconditional branch
    input  [7:0]  issue_branch_target,

    // issue interface
    output [1:0]  issue_warp_id,   // warp being issued
    output [15:0] issue_instr,     // instruction to issue
    output        issue_valid,     // issue is valid
    input         issue_ready,     // exec lanes ready

    // exec completion
    input  [1:0]  complete_warp_id,
    input         complete_valid,
    // barrier
    input         issue_is_barrier, // BARRIER instruction from decode
    // status
    output [3:0]  active_warps     // bitmap of active warps
);
    // barrier sync state
    reg [2:0] barrier_cnt;
    reg [2:0] barrier_expected;
    wire [2:0] popcnt = {1'b0,active_warps[0]} + {1'b0,active_warps[1]} +
                        {1'b0,active_warps[2]} + {1'b0,active_warps[3]};

    localparam ST_IDLE    = 2'b00;
    localparam ST_READY   = 2'b01;
    localparam ST_EXEC    = 2'b10;
    localparam ST_BARRIER = 2'b11;     // waiting at a BARRIER instruction

    // per-warp state
    reg [31:0] pc     [0:NUM_WARPS-1];
    reg [1:0]  status [0:NUM_WARPS-1];

    // round-robin fetch pointer: rotates every cycle
    reg [1:0] rr_ptr;
    always @(posedge clk) begin
        if (reset) rr_ptr <= 2'b0;
        else       rr_ptr <= rr_ptr + 1'b1;
    end

    assign fetch_warp_id = rr_ptr;
    assign fetch_pc      = pc[rr_ptr];

    // issue follows fetch by one cycle (matches gpu_fetch's registered read)
    reg [1:0] issue_id;
    reg       issue_pend;
    always @(posedge clk) begin
        if (reset) begin
            issue_id   <= 2'b0;
            issue_pend <= 1'b0;
        end else begin
            issue_id   <= rr_ptr;
            issue_pend <= (status[rr_ptr] == ST_READY);
        end
    end

    assign issue_warp_id = issue_id;
    assign issue_instr   = fetch_instr;
    // re-check status: a launch/halt may have changed it since the fetch cycle
    assign issue_valid   = issue_pend && fetch_valid && (status[issue_id] == ST_READY);

    integer i;
    always @(posedge clk) begin
        if (reset) begin
            for (i = 0; i < NUM_WARPS; i = i + 1) begin
                pc[i]     <= 32'b0;
                status[i] <= ST_IDLE;
            end
            barrier_cnt <= 3'b0;
            barrier_expected <= 3'b0;
        end else begin
            if (issue_valid && issue_ready) begin
                if (issue_is_halt) begin
                    status[issue_id] <= ST_IDLE;
                end else if (issue_is_barrier) begin
                    status[issue_id] <= ST_BARRIER;
                    pc[issue_id] <= pc[issue_id] + 32'd2;
                    if (barrier_cnt == 3'b0)
                        barrier_expected <= popcnt;
                    barrier_cnt <= barrier_cnt + 3'd1;
                end else begin
                    status[issue_id] <= ST_EXEC;
                    pc[issue_id] <= issue_is_branch ? {24'b0, issue_branch_target}
                                                    : pc[issue_id] + 32'd2;
                end
            end

            // deferred release: one cycle after the last arrival, ST_BARRIER -> ST_READY
            if (barrier_cnt != 3'b0 && barrier_cnt == barrier_expected) begin
                for (i = 0; i < NUM_WARPS; i = i + 1)
                    if (status[i] == ST_BARRIER)
                        status[i] <= ST_READY;
                barrier_cnt <= 3'b0;
                barrier_expected <= 3'b0;
            end

            if (complete_valid && status[complete_warp_id] == ST_EXEC)
                status[complete_warp_id] <= ST_READY;

            // launch last: overrides a completing/halting warp's state
            if (cmd_launch) begin
                pc[cmd_warp_id]     <= cmd_warp_pc;
                status[cmd_warp_id] <= ST_READY;
            end
        end
    end

    assign active_warps = { (status[3] != ST_IDLE), (status[2] != ST_IDLE),
                            (status[1] != ST_IDLE), (status[0] != ST_IDLE) };

endmodule
