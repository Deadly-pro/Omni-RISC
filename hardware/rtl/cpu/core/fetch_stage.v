// fetch_stage.v — IF stage (PC gen + L1 I-cache + fetch buffer)
//
// USE_CACHES=0 (default): fetch reads instr_bram directly (registered read),
// exactly as the pre-Phase-C CPU did — tb_cpu_top and the compliance harness
// (which peek u_fetch->instr_bram1 and rely on fixed run-lengths) stay put.
//
// USE_CACHES=1: fetch goes through l1_icache. On a hit the latency matches the
// direct BRAM read, so no per-instruction stall; a miss asserts icache_miss
// (→ pipeline freeze) while the line refills. instr_bram remains the backing
// memory, driven by the cache's mem_read_req/ack handshake.
module fetch_stage #(
    parameter USE_CACHES = 0
) (
    input clk,
    input reset,
    input stall,
    input redirect_valid,
    input [31:0] redirect_target,
    input trap_valid,
    input [31:0] trap_target,
    output reg [31:0] if_id_pc,
    output reg [31:0] if_id_pc_plus4,
    output [31:0] if_id_instr,
    output icache_miss
);
// flush_q latches a redirect/trap; flush stays high while the redirect is
// STILL active (a branch held in EX by a cache refill keeps redirect_valid=1)
// plus one cycle after, so a wrong-path instruction stranded in IF/ID across a
// multi-cycle freeze cannot come back and retire after the 1-cycle NOP clears.
reg flush_q;
wire flush = reset || redirect_valid || trap_valid || flush_q;
wire [31:0] pc,pc_plus4;
pc_gen pc_gen1(.clk(clk),
.reset(reset),
.stall(stall),
.redirect_valid(redirect_valid),
.redirect_target(redirect_target),
.trap_valid(trap_valid),
.trap_target(trap_target),
.pc(pc),
.pc_plus4(pc_plus4)
);

// backing instruction memory — kept at top level so the compliance harness's
// u_fetch->instr_bram1 peek keeps resolving in BOTH cache configurations
wire [31:0] imem_pc;
wire        imem_en;
wire [31:0] imem_rdata;
instr_bram instr_bram1(
.clk(clk),
.pc(imem_pc),
.en(imem_en),
.rdata(imem_rdata)
);

// the instruction presented to the pipeline (cache read or direct BRAM read)
wire [31:0] fetched;

generate
    if (USE_CACHES) begin : g_icache
        wire [31:0] ic_addr, ic_rdata;
        wire        ic_req, ic_ack;
        // During redirect/trap, icache must present the target's line, not the
        // fetch-ahead pc's line. Redirect_valid is combinational from EX, so
        // ic_pc = redirect_valid ? redirect_target : (trap_valid ? trap_target : pc)
        wire [31:0] ic_pc = redirect_valid ? redirect_target : (trap_valid ? trap_target : pc);
        l1_icache u_icache(
            .clk(clk), .reset(reset),
            // fetch_en = !stall | redirect | trap: a redirect advances IF/ID to the
            // target even during a cache-refill freeze, so the icache must present
            // the target's line too (else IF/ID pairs the target pc with stale rdata)
            .pc(ic_pc), .fetch_en(!stall | redirect_valid | trap_valid),
            .rdata(ic_rdata), .miss(icache_miss),
            .mem_addr(ic_addr), .mem_rdata(imem_rdata),
            .mem_read_req(ic_req), .mem_read_ack(ic_ack)
        );
        // instr_bram is a registered read (rdata valid 1 cycle after en/pc),
        // so the refill ack is the request delayed one cycle
        reg ic_req_d1;
        always @(posedge clk) ic_req_d1 <= ic_req;
        assign ic_ack  = ic_req_d1;
        assign imem_en = ic_req;
        assign imem_pc = ic_addr;
        assign fetched = ic_rdata;
    end else begin : g_nocache
        assign imem_en = !stall;
        assign imem_pc = pc;
        assign icache_miss = 1'b0;
        assign fetched = imem_rdata;
    end
endgenerate

always @(posedge clk)begin
// a redirect/trap wins over any freeze: the fetch-ahead wrong-path must not
// stay stranded in IF/ID across a multi-cycle cache refill, or it comes back
// and retires after the flush NOP clears
// pc_gen's pc updates ONE CYCLE LATE after redirect_valid, so capture the
// redirect_target directly to avoid pairing fetch-ahead pc with target instr
if(redirect_valid)begin
    if_id_pc<=redirect_target;
    if_id_pc_plus4<=redirect_target+4;
    end
else if(trap_valid)begin
    if_id_pc<=trap_target;
    if_id_pc_plus4<=trap_target+4;
    end
else if(!stall)begin
    if_id_pc<=pc;
    if_id_pc_plus4<=pc_plus4;
    end
    flush_q<=reset||redirect_valid||trap_valid;
end
assign if_id_instr=flush?32'h0000_0013:fetched;
endmodule
