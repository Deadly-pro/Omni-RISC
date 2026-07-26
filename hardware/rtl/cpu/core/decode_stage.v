// Omni-RISC APU — CPU Decode: decode_stage
// TODO: Implement

module decode_stage (
    input         clk,
    input         reset,
    input         stall,             // hold ID/EX (hazard unit, later — tie 0 in cpu_top for now)
    input         flush,             // EX took a branch → bubble this instruction

    // ---- IF/ID bundle in (from fetch_stage) ----
    input  [31:0] if_id_pc,
    input  [31:0] if_id_pc_plus4,
    input  [31:0] if_id_instr,

    // ---- WB write port in (from wb_stage, loops backward) ----
    input  [4:0]  wb_rd_addr,
    input  [31:0] wb_rd_data,
    input         wb_reg_write,

    // ---- ID/EX bundle out — data ----
    output reg [31:0] id_ex_pc,
    output reg [31:0] id_ex_pc_plus4,
    output reg [31:0] id_ex_rs1_data,
    output reg [31:0] id_ex_rs2_data,
    output reg [31:0] id_ex_imm,
    output reg [4:0]  id_ex_rd,
    output reg [4:0]  id_ex_rs1_addr,   // for forwarding_net
    output reg [4:0]  id_ex_rs2_addr,   // for forwarding_net

    // ---- ID/EX bundle out — control ----
    output reg [3:0]  id_ex_alu_op,
    output reg [1:0]  id_ex_op_type,
    output reg [2:0]  id_ex_funct3,
    output reg        id_ex_branch,
    output reg        id_ex_jump,
    output reg        id_ex_reg_write,
    output reg        id_ex_mem_read,
    output reg        id_ex_mem_write,
    output reg        id_ex_is_mul_div
);
reg [4:0] rs1,rs2,rd;
reg [31:0] immediate,rs1_data,rs2_data;
reg [2:0] funct3;
reg [6:0] funct7;
reg reg_write,mem_read,mem_write,branch,jump,is_mul_div,illegal_instr;
reg [3:0] alu_op;
reg [1:0] op_type;
decoder decoder1(
    .instruction(if_id_instr),
    .rs1(rs1),.rs2(rs2),.rd(rd),
    .immediate(immediate),.alu_op(alu_op),
    .funct3(funct3),.funct7(funct7),
    .reg_write(reg_write),.mem_read(mem_read),
    .mem_write(mem_write),.branch(branch),.jump(jump),
    .is_mul_div(is_mul_div),.op_type(op_type),.illegal_instr(illegal_instr)
);
regfile regfile1(
    .clk(clk),
    .reset(reset),
    .rs1_addr(rs1),.rs2_addr(rs2),
    .rd_addr(wb_rd_addr),.rd_data(wb_rd_data),
    .rd_write_en(wb_reg_write),
    .rs1_data(rs1_data),
    .rs2_data(rs2_data)
);
always @(posedge clk)begin
    if(reset||stall||flush)begin
        id_ex_pc<=0;
        id_ex_pc_plus4<=0;
        id_ex_imm<=0;
        id_ex_reg_write<=0;
        id_ex_mem_read<=0;
        id_ex_mem_write<=0;
        id_ex_branch<=0;
        id_ex_jump<=0;
        id_ex_rs1_data<=0;
        id_ex_rs2_data<=0;
        id_ex_funct3<=0;
        id_ex_is_mul_div<=0;
    end
    else begin
        id_ex_pc<=if_id_pc;
        id_ex_pc_plus4<=if_id_pc_plus4;
        id_ex_imm<=immediate;
        id_ex_rd<=rd;
        id_ex_rs1_addr<=rs1;
        id_ex_rs2_addr<=rs2;
        id_ex_alu_op<=alu_op;
        id_ex_op_type<=op_type;
        id_ex_branch<=branch;
        id_ex_jump<=jump;
        id_ex_reg_write<=reg_write;
        id_ex_mem_read<=mem_read;
        id_ex_mem_write<=mem_write;
        id_ex_rs1_data<=rs1_data;
        id_ex_rs2_data<=rs2_data;
        id_ex_funct3<=funct3;
        id_ex_is_mul_div<=is_mul_div;
    end

end
endmodule
