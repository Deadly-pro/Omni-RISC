// Omni-RISC APU — CPU: cpu_top
// TODO: Implement
module cpu_top (
      input clk,
      input reset
  );
wire stall,bubble;
wire trap_valid=1'b0;
wire [31:0] trap_target=32'b0;
wire redirect_valid;
wire [31:0] redirect_target,if_id_pc,if_id_pc_plus4,if_id_instr;

fetch_stage u_fetch(
    .clk(clk),
    .reset(reset),
    .stall(freeze),
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
wire [4:0]  id_ex_rd,id_ex_rs1_addr,id_ex_rs2_addr;   
wire id_ex_is_mul_div;
wire [3:0]  id_ex_alu_op;
wire [1:0]  id_ex_op_type;
wire [2:0]  id_ex_funct3;
wire id_ex_branch,id_ex_jump,id_ex_reg_write,id_ex_mem_read,id_ex_mem_write,id_ex_is_csr;
wire div_stall;                    
wire freeze = stall | div_stall;
decode_stage u_decode(
    .clk(clk),
    .reset(reset),
    .stall(stall), 
    .hold(div_stall),
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
    .id_ex_mem_write(id_ex_mem_write),
    .id_ex_is_mul_div(id_ex_is_mul_div),
    .id_ex_is_csr(id_ex_is_csr)
);
wire [4:0] if_id_rs1 = if_id_instr[19:15];
wire [4:0] if_id_rs2 = if_id_instr[24:20];
hazard_unit u_hazard(
      .if_id_rs1(if_id_rs1),
      .if_id_rs2(if_id_rs2),
      .id_ex_rd(id_ex_rd),
      .id_ex_mem_read(id_ex_mem_read),
      .stall(stall),
      .bubble(bubble)
  );
 wire [31:0] ex_mem_alu_result,ex_mem_store_data,ex_mem_pc_plus4;
 wire [4:0]  ex_mem_rd;
 wire [2:0]  ex_mem_funct3;   
 wire ex_mem_jump,ex_mem_reg_write,ex_mem_mem_read,ex_mem_mem_write;
 
exec_stage u_exec(
    .clk(clk),
    .reset(reset),
    .stall(1'b0), // step-8: EX gains real stall on M-ext (div busy) — see roadmap
    .div_stall(div_stall),
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
    .id_ex_is_csr(id_ex_is_csr),
    .redirect_valid(redirect_valid),
    .redirect_target(redirect_target),
    .id_ex_rs1_addr(id_ex_rs1_addr),
    .id_ex_rs2_addr(id_ex_rs2_addr),
    .id_ex_is_mul_div(id_ex_is_mul_div),
    .mem_wb_rd(mem_wb_rd),
    .mem_wb_reg_write(mem_wb_reg_write),
    .wb_rd_data(wb_rd_data),
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
wire [31:0] mem_wb_alu_result,mem_wb_rdata,mem_wb_pc_plus4;
wire [4:0]  mem_wb_rd;
wire mem_wb_jump,mem_wb_mem_read,mem_wb_reg_write;
wire [1:0] mem_wb_ld_lsb;
wire [2:0] mem_wb_ld_funct3;
mem_stage u_mem(
    .clk(clk),
    .reset(reset),
    .stall(1'b0), // phase-C: MEM gains real stall on cache miss — see roadmap

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
    .mem_wb_rdata(mem_wb_rdata),
    .mem_wb_rd(mem_wb_rd),
    .mem_wb_pc_plus4(mem_wb_pc_plus4),
    .mem_wb_jump(mem_wb_jump),
    .mem_wb_mem_read(mem_wb_mem_read),
    .mem_wb_reg_write(mem_wb_reg_write),
    .mem_wb_ld_lsb(mem_wb_ld_lsb),
    .mem_wb_ld_funct3(mem_wb_ld_funct3)
  );

wb_stage u_wb(
    .mem_wb_alu_result(mem_wb_alu_result),
    .mem_wb_rdata(mem_wb_rdata),
    .mem_wb_rd(mem_wb_rd),
    .mem_wb_pc_plus4(mem_wb_pc_plus4),
    .mem_wb_jump(mem_wb_jump),
    .mem_wb_mem_read(mem_wb_mem_read),
    .mem_wb_reg_write(mem_wb_reg_write),
    .mem_wb_ld_lsb(mem_wb_ld_lsb),
    .mem_wb_ld_funct3(mem_wb_ld_funct3),

    .wb_rd_addr(wb_rd_addr),
    .wb_rd_data(wb_rd_data),
    .wb_reg_write(wb_reg_write)
);
endmodule 
