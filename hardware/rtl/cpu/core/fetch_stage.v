// fetch_stage.v — IF stage (PC gen + I-cache interface + fetch buffer)
module fetch_stage (
    input clk,
    input reset,
    input stall,
    input redirect_valid,
    input [31:0] redirect_target,
    input trap_valid,
    input [31:0] trap_target,
    output reg [31:0] if_id_pc,
    output reg [31:0] if_id_pc_plus4,
    output [31:0] if_id_instr
);
reg flush;
reg [31:0] bram_rdata;
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
instr_bram instr_bram1(
.clk(clk),
.pc(pc),
.en(!stall),
.rdata(bram_rdata)
);

always @(posedge clk)begin
if(!stall)begin
    if_id_pc<=pc;
    if_id_pc_plus4<=pc_plus4;
    end
    flush<=reset||redirect_valid||trap_valid;
end
assign if_id_instr=flush?32'h0000_0013:bram_rdata;
endmodule
