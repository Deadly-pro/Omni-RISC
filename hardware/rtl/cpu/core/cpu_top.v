// Omni-RISC APU — CPU: cpu_top
module cpu_top #(
    parameter DMEM_FILE = "",      // SoC sets the firmware image; bare-CPU tests leave empty
    parameter USE_CACHES = 0       // 0 = direct BRAMs (tb_cpu_top/compliance), 1 = L1 caches (SoC)
) (
      input clk,
      input reset,

      // ---- peripheral bus (to SoC slaves: UART/TIMER/GPIO) ----
      output [31:0] pbus_addr,
      output [31:0] pbus_wdata,
      output [3:0]  pbus_wen,
      output        pbus_read,
      input  [31:0] pbus_rdata,
      // ---- machine timer interrupt pending (from SoC CLINT) ----
      input         mtip,
      // ---- snoop interface (for dual-core MSI coherence) ----
      input  [31:0] snoop_addr,
      input         snoop_read,
      input         snoop_write,
      output        snoop_ack
  );
wire stall,bubble;
wire trap_valid;
wire [31:0] trap_target;
wire redirect_valid;
wire [31:0] redirect_target,if_id_pc,if_id_pc_plus4,if_id_instr;
wire icache_miss, mem_stall;

fetch_stage #(.USE_CACHES(USE_CACHES)) u_fetch(
    .clk(clk),
    .reset(reset),
    .stall(freeze),
    .redirect_valid(redirect_valid),
    .redirect_target(redirect_target),
    .trap_valid(trap_valid),
    .trap_target(trap_target),
    .if_id_pc(if_id_pc),
    .if_id_pc_plus4(if_id_pc_plus4),
    .if_id_instr(if_id_instr),
    .icache_miss(icache_miss)
);
wire flush=redirect_valid | trap_valid;
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
wire id_ex_is_ecall,id_ex_is_ebreak,id_ex_is_mret,id_ex_illegal;
wire div_stall;
// A cache refill (I or D) freezes every stage exactly like div_stall: the
// stalled instruction must not retire, and its EX/MEM bundle must not be
// clobbered by whatever decode/exec held.
wire freeze = stall | div_stall | icache_miss | mem_stall;
decode_stage u_decode(
    .clk(clk),
    .reset(reset),
    .stall(stall),
    .hold(div_stall | icache_miss | mem_stall),
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
    .id_ex_is_csr(id_ex_is_csr),
    .id_ex_is_ecall(id_ex_is_ecall),
    .id_ex_is_ebreak(id_ex_is_ebreak),
    .id_ex_is_mret(id_ex_is_mret),
    .id_ex_illegal(id_ex_illegal)
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
    .stall(mem_stall | icache_miss), // phase-C: hold EX/MEM during any cache refill;
                                     // without it EX re-executes the instruction
                                     // frozen in ID/EX on every refill cycle
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
    .id_ex_is_ecall(id_ex_is_ecall),
    .id_ex_is_ebreak(id_ex_is_ebreak),
    .id_ex_is_mret(id_ex_is_mret),
    .id_ex_illegal(id_ex_illegal),
    .mtip(mtip),
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
    .ex_mem_mem_write(ex_mem_mem_write),
    .trap_valid(trap_valid),
    .trap_target(trap_target)
);
wire [31:0] mem_wb_alu_result,mem_wb_rdata,mem_wb_pc_plus4;
wire [4:0]  mem_wb_rd;
wire mem_wb_jump,mem_wb_mem_read,mem_wb_reg_write;
wire [1:0] mem_wb_ld_lsb;
wire [2:0] mem_wb_ld_funct3;
mem_stage #(.DMEM_FILE(DMEM_FILE), .USE_CACHES(USE_CACHES)) u_mem(
    .clk(clk),
    .reset(reset),
    .stall(1'b0), // MEM holds itself via mem_stall while a cache op is pending

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
    .mem_wb_ld_funct3(mem_wb_ld_funct3),

      // ---- peripheral bus ----
    .pbus_addr(pbus_addr),
    .pbus_wdata(pbus_wdata),
    .pbus_wen(pbus_wen),
    .pbus_read(pbus_read),
    .pbus_rdata(pbus_rdata),

      // ---- L1 D-cache freeze ----
    .mem_stall(mem_stall),

      // ---- snoop interface ----
    .snoop_addr(snoop_addr),
    .snoop_read(snoop_read),
    .snoop_write(snoop_write),
    .snoop_ack(snoop_ack)
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
