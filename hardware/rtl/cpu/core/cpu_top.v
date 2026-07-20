// Omni-RISC APU — CPU: cpu_top
// TODO: Implement
module cpu_top (
      input clk,
      input reset
  );
wire stall=1'b0;
wire trap_valid=1'b0;
wire [31:0] trap_target=32'b0;
wire redirect_valid;
wire [31:0] redirect_target,if_id_pc,if_id_pc_plus4,if_id_instr;

fetch_stage u_fetch(
    .clk(clk),
    .reset(reset),
    .stall(stall),
    .redirect_valid(redirect_valid),
    .redirect_target(redirect_target),
    .trap_valid(trap_valid),
    .trap_target(trap_target),
    .if_id_pc(if_id_pc),
    .if_id_pc_plus4(if_id_pc_plus4),
    .if_id_instr(if_id_instr)
);
wire flush=redirect_valid; //for test run we let it be 0
//loopback from wb stage
wire [4:0] wb_rd_addr;
wire [31:0] wb_rd_data;
wire wb_reg_write; 
wire [31:0] id_ex_pc,id_ex_pc_plus4,id_ex_rs1_data,id_ex_rs2_data,id_ex_imm;
wire [4:0]  id_ex_rd,id_ex_rs1_addr,id_ex_rs2_addr;   // for forwarding_net later

wire [3:0]  id_ex_alu_op;
wire [1:0]  id_ex_op_type;
wire [2:0]  id_ex_funct3;
wire id_ex_branch,id_ex_jump,id_ex_reg_write,id_ex_mem_read,id_ex_mem_write;

decode_stage u_decode(
    .clk(clk),
    .reset(reset),
    .stall(stall),
    .flush(flush),
    .if_id_pc(if_id_pc),
    .if_id_pc_plus4(if_id_pc_plus4),
    .if_id_instr(if_id_instr),
    .wb_rd_addr(wb_rd_addr),
    .wb_rd_data(wb_rd_data),
    .wb_reg_write(wb_reg_write),
    .id_ex_pc(id_ex_pc),
    .id_ex_pc_plus4(id_ex_pc_plus4),
    .id_ex_rs1_data(id_ex_rs1_data),
    .id_ex_rs2_data(id_ex_rs2_data),
    .id_ex_imm(id_ex_imm),
    .id_ex_rd(id_ex_rd),
    .id_ex_rs1_addr(id_ex_rs1_addr),
    .id_ex_rs2_addr(id_ex_rs2_addr),
    .id_ex_alu_op(id_ex_alu_op),
    .id_ex_op_type(id_ex_op_type),
    .id_ex_funct3(id_ex_funct3),
    .id_ex_branch(id_ex_branch),
    .id_ex_jump(id_ex_jump),
    .id_ex_reg_write(id_ex_reg_write),
    .id_ex_mem_read(id_ex_mem_read),
    .id_ex_mem_write(id_ex_mem_write)
);
 wire [31:0] ex_mem_alu_result,ex_mem_store_data,ex_mem_pc_plus4;
 wire [4:0]  ex_mem_rd;
 wire [2:0]  ex_mem_funct3;   
 wire ex_mem_jump,ex_mem_reg_write,ex_mem_mem_read,ex_mem_mem_write;
 
exec_stage u_exec(
    .clk(clk),
    .reset(reset),
    .stall(stall),
    .id_ex_pc(id_ex_pc),
    .id_ex_pc_plus4(id_ex_pc_plus4),
    .id_ex_rs1_data(id_ex_rs1_data),
    .id_ex_rs2_data(id_ex_rs2_data),
    .id_ex_imm(id_ex_imm),
    .id_ex_rd(id_ex_rd),
    .id_ex_alu_op(id_ex_alu_op),
    .id_ex_op_type(id_ex_op_type),
    .id_ex_funct3(id_ex_funct3),
    .id_ex_branch(id_ex_branch),
    .id_ex_jump(id_ex_jump),
    .id_ex_reg_write(id_ex_reg_write),
    .id_ex_mem_read(id_ex_mem_read),
    .id_ex_mem_write(id_ex_mem_write),
    .redirect_valid(redirect_valid),
    .redirect_target(redirect_target),

    .ex_mem_alu_result(ex_mem_alu_result),
    .ex_mem_store_data(ex_mem_store_data),
    .ex_mem_rd(ex_mem_rd),
    .ex_mem_funct3(ex_mem_funct3),
    .ex_mem_pc_plus4(ex_mem_pc_plus4),
    .ex_mem_jump(ex_mem_jump),
    .ex_mem_reg_write(ex_mem_reg_write),
    .ex_mem_mem_read(ex_mem_mem_read),
    .ex_mem_mem_write(ex_mem_mem_write)
);
wire [31:0] mem_wb_alu_result,mem_wb_load_data,mem_wb_pc_plus4;
wire [4:0]  mem_wb_rd;
wire mem_wb_jump,mem_wb_mem_read,mem_wb_reg_write;

mem_stage u_mem(
    .clk(clk),
    .reset(reset),
    .stall(stall),

    .ex_mem_alu_result(ex_mem_alu_result),
    .ex_mem_store_data(ex_mem_store_data),
    .ex_mem_rd(ex_mem_rd),
    .ex_mem_funct3(ex_mem_funct3),
    .ex_mem_pc_plus4(ex_mem_pc_plus4),
    .ex_mem_jump(ex_mem_jump),
    .ex_mem_reg_write(ex_mem_reg_write),
    .ex_mem_mem_read(ex_mem_mem_read),
    .ex_mem_mem_write(ex_mem_mem_write),

      // ---- MEM/WB bundle out ----
    .mem_wb_alu_result(mem_wb_alu_result),
    .mem_wb_load_data(mem_wb_load_data),
    .mem_wb_rd(mem_wb_rd),
    .mem_wb_pc_plus4(mem_wb_pc_plus4),
    .mem_wb_jump(mem_wb_jump),
    .mem_wb_mem_read(mem_wb_mem_read),
    .mem_wb_reg_write(mem_wb_reg_write)
  );

wb_stage u_wb(
    .mem_wb_alu_result(mem_wb_alu_result),
    .mem_wb_load_data(mem_wb_load_data),
    .mem_wb_rd(mem_wb_rd),
    .mem_wb_pc_plus4(mem_wb_pc_plus4),
    .mem_wb_jump(mem_wb_jump),
    .mem_wb_mem_read(mem_wb_mem_read),
    .mem_wb_reg_write(mem_wb_reg_write),

    .wb_rd_addr(wb_rd_addr),
    .wb_rd_data(wb_rd_data),
    .wb_reg_write(wb_reg_write)
);
endmodule 
