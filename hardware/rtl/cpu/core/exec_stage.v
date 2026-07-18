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
     output reg        ex_mem_mem_write
 );

reg [31:0] operand_a,operand_b,result;
reg zero_flag,take_branch;
assign operand_a=(id_ex_alu_op==11 ||id_ex_branch||(id_ex_jump && id_ex_op_type==0))?id_ex_pc:id_ex_rs1_data;
assign operand_b=(id_ex_op_type==0 && !id_ex_jump)?id_ex_rs2_data:id_ex_imm;
alu alu1(
.operand_a(operand_a),
.operand_b(operand_b),
.alu_op(id_ex_alu_op),
.result(result),
.zero_flag(zero_flag)
);
branch_unit branch_unit1(
.rs1_data(id_ex_rs1_data),
.rs2_data(id_ex_rs2_data),
.funct3(id_ex_funct3),
.is_branch(id_ex_branch),
.is_jump(id_ex_jump),
.take_branch(take_branch)
);
assign redirect_valid=take_branch;
assign redirect_target=result&~32'h1;
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
    end
    else if(!stall)begin
        ex_mem_alu_result<=result;
        ex_mem_store_data<=id_ex_rs2_data;
        ex_mem_rd<=id_ex_rd;
        ex_mem_funct3<=id_ex_funct3;
        ex_mem_pc_plus4<=id_ex_pc_plus4;
        ex_mem_jump<=id_ex_jump;
        ex_mem_reg_write<=id_ex_reg_write;
        ex_mem_mem_read<=id_ex_mem_read;
        ex_mem_mem_write<=id_ex_mem_write;
    end
end
endmodule
