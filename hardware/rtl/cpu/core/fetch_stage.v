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
reg flush;
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
        l1_icache u_icache(
            .clk(clk), .reset(reset),
            .pc(pc), .fetch_en(!stall), .rdata(ic_rdata), .miss(icache_miss),
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
if(!stall)begin
    if_id_pc<=pc;
    if_id_pc_plus4<=pc_plus4;
    end
    flush<=reset||redirect_valid||trap_valid;
end
assign if_id_instr=flush?32'h0000_0013:fetched;
endmodule
