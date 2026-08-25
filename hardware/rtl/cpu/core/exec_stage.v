// Omni-RISC APU — CPU Execute: exec_stage
// TODO: Implement
module exec_stage (
     input         clk,
     input         reset,
     input         stall,
     // ---- ID/EX bundle in (from decode_stage, names match 1:1) ----
     input  [31:0] id_ex_pc,
     input  [31:0] id_ex_pc_plus4,
     input  [31:0] id_ex_rs1_data,
     input  [31:0] id_ex_rs2_data,
     input  [31:0] id_ex_imm,
     input  [4:0]  id_ex_rd,
     input  [3:0]  id_ex_alu_op,
     input  [1:0]  id_ex_op_type,
     input  [2:0]  id_ex_funct3,
     input         id_ex_branch,
     input         id_ex_jump,
     input         id_ex_reg_write,
     input         id_ex_mem_read,
     input         id_ex_mem_write,
     input         id_ex_is_mul_div,
     input         id_ex_is_atomic,    // A-extension LR/SC
     input         id_ex_is_csr,
     input         id_ex_is_ecall,     // to trap_unit
     input         id_ex_is_ebreak,    // to trap_unit
     input         id_ex_is_mret,      // to trap_unit
     input         id_ex_illegal,      // to trap_unit
     input         mtip,               // machine timer interrupt pending (from SoC)
     input         msip,               // machine software interrupt pending (from CLINT)
     // ---- forwarding inputs ----
     input  [4:0]  id_ex_rs1_addr,     // NEW — match key
     input  [4:0]  id_ex_rs2_addr,     // NEW — match key
     input  [4:0]  mem_wb_rd,          // NEW — distance-2 producer id
     input         mem_wb_reg_write,   // NEW — distance-2 producer writes?
     input  [31:0] wb_rd_data,         // NEW — distance-2 forward value (from wb_stage)
        // ---- Redirect out — COMBINATIONAL, loops back to fetch + decode.flush ----
     output        redirect_valid,
     output [31:0] redirect_target,

     // ---- EX/MEM bundle out ----
     output reg [31:0] ex_mem_alu_result,
     output reg [31:0] ex_mem_store_data,   // rs2_data, renamed: its only job now is SW
     output reg [4:0]  ex_mem_rd,
     output reg [2:0]  ex_mem_funct3,       // load/store size, later
     output reg [31:0] ex_mem_pc_plus4,     // JAL/JALR link value → WB
     output reg        ex_mem_jump,         // tells WB to write pc_plus4, not alu_result
     output reg        ex_mem_reg_write,
     output reg        ex_mem_mem_read,
     output reg        ex_mem_mem_write,
     output reg        ex_mem_is_lr,        // A-extension LR.W
     output reg        ex_mem_is_sc,        // A-extension SC.W
     output            div_stall,
        // ---- Trap/mret out — COMBINATIONAL, loops back to fetch + decode.flush ----
     output            trap_valid,
     output [31:0]     trap_target
 );

wire [31:0] operand_a,operand_b,result;
wire zero_flag,take_branch;
wire [31:0] fwd_rs1_data, fwd_rs2_data;
wire [31:0] ex_mem_fwd_value = ex_mem_jump ? ex_mem_pc_plus4 : ex_mem_alu_result;
forwarding_net fwd1(
      .id_ex_rs1_addr(id_ex_rs1_addr),
      .id_ex_rs2_addr(id_ex_rs2_addr),
      .id_ex_rs1_data(id_ex_rs1_data),
      .id_ex_rs2_data(id_ex_rs2_data),
      .ex_mem_reg_write(ex_mem_reg_write),
      .ex_mem_fwd_value(ex_mem_fwd_value),
      .ex_mem_rd(ex_mem_rd),
      .mem_wb_reg_write(mem_wb_reg_write),
      .mem_wb_fwd_value(wb_rd_data),
      .mem_wb_rd(mem_wb_rd),
      .fwd_rs1_data(fwd_rs1_data),
      .fwd_rs2_data(fwd_rs2_data)
  );

assign operand_a=(id_ex_alu_op==11 ||id_ex_branch||(id_ex_jump && id_ex_op_type==0))?id_ex_pc:fwd_rs1_data;
assign operand_b=(id_ex_op_type==0 && !id_ex_jump&& id_ex_alu_op!=10 && id_ex_alu_op!=11)?fwd_rs2_data:id_ex_imm;
alu alu1(
.operand_a(operand_a),
.operand_b(operand_b),
.alu_op(id_ex_alu_op),
.result(result),
.zero_flag(zero_flag)
);
branch_unit branch_unit1(
.rs1_data(fwd_rs1_data),
.rs2_data(fwd_rs2_data),
.funct3(id_ex_funct3),
.is_branch(id_ex_branch),
.is_jump(id_ex_jump),
.take_branch(take_branch)
);
wire [31:0] csr_rdata;
wire        csr_illegal;
wire [31:0] csr_mtvec_o, csr_mepc_o, csr_mstatus_o, csr_mie_o, csr_mip_o;
wire        trap_taken, trap_mret;
wire [31:0] trap_pc, trap_cause, trap_tval;
csr_file csr_file1(
    .clk(clk),
    .reset(reset),
    .csr_addr(id_ex_imm[11:0]),
    .csr_funct3(id_ex_funct3),
    .csr_wdata(fwd_rs1_data),
    .csr_uimm(id_ex_rs1_addr),
    .csr_en(id_ex_is_csr),
    .csr_rdata(csr_rdata),
    .csr_illegal(csr_illegal),
    .trap_taken(trap_taken),
    .trap_pc(trap_pc),
    .trap_cause(trap_cause),
    .trap_tval(trap_tval),
    .mret(trap_mret),
    .mtip(mtip),
    .msip(msip),
    .mtvec_o(csr_mtvec_o),
    .mepc_o(csr_mepc_o),
    .mstatus_o(csr_mstatus_o),
    .mie_o(csr_mie_o),
    .mip_o(csr_mip_o)
);
trap_unit trap1(
    .id_ex_is_ecall(id_ex_is_ecall),
    .id_ex_is_ebreak(id_ex_is_ebreak),
    .id_ex_illegal(id_ex_illegal),
    .id_ex_is_csr(id_ex_is_csr),
    .csr_illegal(csr_illegal),
    .id_ex_is_mret(id_ex_is_mret),
    .id_ex_pc(id_ex_pc),
    .mtvec(csr_mtvec_o),
    .mepc(csr_mepc_o),
    .mstatus_mie(csr_mstatus_o[3]),
    .mie_mtie(csr_mie_o[7]),
    .mip_mtip(csr_mip_o[7]),
    .mie_msie(csr_mie_o[3]),
    .mip_msip(csr_mip_o[3]),
    .ex_mem_mem_read(ex_mem_mem_read),
    .redirect_pending(redirect_pending),
    .redirect_target(redirect_target_q),
    .trap_valid(trap_valid),
    .trap_target(trap_target),
    .trap_pc(trap_pc),
    .trap_cause(trap_cause),
    .trap_tval(trap_tval),
    .trap_taken(trap_taken),
    .mret(trap_mret)
);
wire [31:0] mul_result;
  multiplier mul1(
    .operand_a(fwd_rs1_data),   // forwarded — NOT operand_a
    .operand_b(fwd_rs2_data),   // forwarded — NOT operand_b
    .funct3(id_ex_funct3),
    .result(mul_result)
  );
wire is_mul = id_ex_is_mul_div & ~id_ex_funct3[2];
assign redirect_valid=take_branch;
assign redirect_target=result&~32'h1;

// A branch/jump redirect drains wrong-path (fetch-ahead) instructions for a
// few cycles. An interrupt taken during that drain must not capture a
// wrong-path pc as mepc (mret would jump into garbage). Track the last
// redirect target and a short pending window so the interrupt's mepc can fall
// back to the redirect target (the valid continuation point).
//
// NOTE: redirect_pending deliberately EXCLUDES the same-cycle redirect_valid.
// On the cycle the redirect fires, the registered redirect_target_q still
// holds the PREVIOUS redirect's target (it updates at that same edge); if an
// interrupt latched mepc from it, mret would jump into the stale target (the
// FreeRTOS tick-during-tail-jump crash). On that cycle id_ex_pc is the branch
// itself, so mepc = id_ex_pc is correct (mret re-executes the branch). Only
// the DRAIN cycles that follow (win != 0) need the redirect target, and by
// then redirect_target_q has settled.
reg [1:0]  redirect_win;
reg [31:0] redirect_target_q;
always @(posedge clk) begin
    if (reset) begin
        redirect_win      <= 2'b00;
        redirect_target_q <= 32'b0;
    end else if (redirect_valid) begin
        redirect_win      <= 2'b11;
        redirect_target_q <= redirect_target;
    end else if (redirect_win != 2'b00) begin
        redirect_win <= redirect_win - 1'b1;
    end
end
wire redirect_pending = (redirect_win != 2'b00);

wire [31:0] div_result;  wire div_busy, div_done;
wire is_div    = id_ex_is_mul_div & id_ex_funct3[2];
wire is_atomic = id_ex_is_atomic;
wire div_start = is_div & ~div_busy & ~div_done;
divider div1(.clk(clk), .reset(reset), .start(div_start),
             .operand_a(fwd_rs1_data), .operand_b(fwd_rs2_data),
             .funct3(id_ex_funct3), .result(div_result),
             .busy(div_busy), .done(div_done));
assign div_stall = is_div & ~div_done;
always @(posedge clk) begin
    if(reset)begin
     ex_mem_alu_result<=0;
     ex_mem_store_data<=0;
     ex_mem_rd<=0;
     ex_mem_funct3<=0;
     ex_mem_pc_plus4<=0;
     ex_mem_jump<=0;
     ex_mem_reg_write<=0;
     ex_mem_mem_read<=0;
     ex_mem_mem_write<=0;
     ex_mem_is_lr<=0;
     ex_mem_is_sc<=0;
    end
    else if (is_div & ~div_done) begin
    ex_mem_reg_write <= 0; ex_mem_mem_read <= 0; ex_mem_mem_write <= 0;
    end
    else if (trap_valid) begin
    // trapping instruction must not retire: no writeback, no mem access
    ex_mem_reg_write <= 0; ex_mem_mem_read <= 0; ex_mem_mem_write <= 0;
    end
    else if(!stall)begin
        ex_mem_alu_result <= id_ex_is_csr ? csr_rdata : (is_div ? div_result : (is_mul ? mul_result : result));
        ex_mem_store_data<=fwd_rs2_data;
        ex_mem_rd<=id_ex_rd;
        ex_mem_funct3<=id_ex_funct3;
        ex_mem_pc_plus4<=id_ex_pc_plus4;
        ex_mem_jump<=id_ex_jump;
        ex_mem_reg_write<=id_ex_reg_write;
        // SC.W is a store but also returns a result (0=success,1=fail) that must
        // ride the load path so WB writes the dcache's rdata, not the ALU result
        ex_mem_mem_read<=id_ex_mem_read | (is_atomic & id_ex_mem_write);
        ex_mem_mem_write<=id_ex_mem_write;
        ex_mem_is_lr<=id_ex_mem_read & is_atomic;  // LR.W is a load
        ex_mem_is_sc<=id_ex_mem_write & is_atomic;  // SC.W is a store
    end
end
endmodule
