// Omni-RISC APU — CPU Decode: decode_stage
// TODO: Implement

module decode_stage (
    input         clk,
    input         reset,
    input         stall,             // hold ID/EX (hazard unit, later — tie 0 in cpu_top for now)
    input         flush,             // EX took a branch → bubble this instruction
    input         hold,
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
    output reg        id_ex_is_csr,
    // ---- ID/EX bundle out — control ----
    output reg [3:0]  id_ex_alu_op,
    output reg [1:0]  id_ex_op_type,
    output reg [2:0]  id_ex_funct3,
    output reg        id_ex_branch,
    output reg        id_ex_jump,
    output reg        id_ex_reg_write,
    output reg        id_ex_mem_read,
    output reg        id_ex_mem_write,
    output reg        id_ex_is_mul_div,
    output reg        id_ex_is_atomic,   // A-extension LR/SC
    output reg        id_ex_is_ecall,   // to trap_unit
    output reg        id_ex_is_ebreak,  // to trap_unit
    output reg        id_ex_is_mret,    // to trap_unit
    output reg        id_ex_illegal     // to trap_unit
);
wire [4:0] rs1,rs2,rd;
wire [31:0] immediate,rs1_data,rs2_data;
wire [2:0] funct3;
wire [6:0] funct7;
wire reg_write,mem_read,mem_write,branch,jump,is_mul_div,is_atomic,illegal_instr,is_csr,is_ecall,is_ebreak,is_mret;
wire [3:0] alu_op;
wire [1:0] op_type;
decoder decoder1(
    .instruction(if_id_instr),
    .rs1(rs1),.rs2(rs2),.rd(rd),
    .immediate(immediate),.alu_op(alu_op),
    .funct3(funct3),.funct7(funct7),
    .reg_write(reg_write),.mem_read(mem_read),
    .mem_write(mem_write),.branch(branch),.jump(jump),
    .is_mul_div(is_mul_div),.is_atomic(is_atomic),.op_type(op_type),.illegal_instr(illegal_instr),.is_csr(is_csr),
    .is_ecall(is_ecall),.is_ebreak(is_ebreak),.is_mret(is_mret)
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
    if(reset||stall)begin
        // reset / hazard load-use stall → inject a bubble
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
        id_ex_is_atomic<=0;
        id_ex_is_csr<=0;
        id_ex_is_ecall<=0;
        id_ex_is_ebreak<=0;
        id_ex_is_mret<=0;
        id_ex_illegal<=0;
    end
    else if (hold)begin
        // Pipeline freeze (div_stall / cache refill / data_bram read hold):
        // hold the ID/EX bundle EVEN IF a flush is pending. The flushing
        // instruction (branch/jump) is still in EX and frozen by the same
        // freeze — its EX/MEM capture is deferred, so clearing ID/EX here
        // would destroy the bundle before EX/MEM can capture it (the jump's
        // link value would be lost). It re-issues its redirect on release.
        // do nothing ???
    end
    else if (flush)begin
        // normal branch/jump redirect → bubble the next instruction
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
        id_ex_is_atomic<=0;
        id_ex_is_csr<=0;
        id_ex_is_ecall<=0;
        id_ex_is_ebreak<=0;
        id_ex_is_mret<=0;
        id_ex_illegal<=0;
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
        id_ex_is_atomic<=is_atomic;
        id_ex_is_csr<=is_csr;
        id_ex_is_ecall<=is_ecall;
        id_ex_is_ebreak<=is_ebreak;
        id_ex_is_mret<=is_mret;
        id_ex_illegal<=illegal_instr;
    end

end
endmodule
