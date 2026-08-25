// Omni-RISC APU — CPU: forwarding_net
//
// EX operand forwarding: bypasses distance-1 (EX/MEM) and distance-2
// (MEM/WB) results over the regfile read values. Distance-1 wins ties
// (newest value). Jump-link results forward pc+4, not the ALU result.
// Loads cannot forward from EX/MEM (data not valid until the mem_hold
// cycle) — that case is a hazard-unit stall instead.
 module forwarding_net (
      // ---- who the current EX instruction wants to read ----
      input  [4:0]  id_ex_rs1_addr,
      input  [4:0]  id_ex_rs2_addr,
      input  [31:0] id_ex_rs1_data,     // baseline value from regfile (fallback)
      input  [31:0] id_ex_rs2_data,     // baseline value from regfile (fallback)

      // ---- producer 1 ahead: the EX/MEM register (distance-1) ----
      input  [4:0]  ex_mem_rd,
      input         ex_mem_reg_write,
      input  [31:0] ex_mem_fwd_value,   // = ex_mem_jump ? ex_mem_pc_plus4 : ex_mem_alu_result

      // ---- producer 2 ahead: the MEM/WB register (distance-2) ----
      input  [4:0]  mem_wb_rd,
      input         mem_wb_reg_write,
      input  [31:0] mem_wb_fwd_value,   // = wb_rd_data (the value hitting the regfile write port)

      // ---- resolved operands for EX to use instead of id_ex_rs*_data ----
      output reg [31:0] fwd_rs1_data,
      output reg [31:0] fwd_rs2_data
  );
 always @(*) begin
    if(id_ex_rs1_addr!=5'b0000 && id_ex_rs1_addr==ex_mem_rd && ex_mem_reg_write) // RS1 distance 1 forward
        fwd_rs1_data=ex_mem_fwd_value;
    else if (id_ex_rs1_addr!=5'b0000 && id_ex_rs1_addr==mem_wb_rd && mem_wb_reg_write) // RS1 distance 2 forward
        fwd_rs1_data=mem_wb_fwd_value;
    else begin 
        fwd_rs1_data=id_ex_rs1_data;
    end

    if(id_ex_rs2_addr!=5'b0000 && id_ex_rs2_addr==ex_mem_rd && ex_mem_reg_write) // RS2 distance 1 forward
        fwd_rs2_data=ex_mem_fwd_value;
    else if(id_ex_rs2_addr!=5'b0000 && id_ex_rs2_addr==mem_wb_rd && mem_wb_reg_write) // RS2 distance 2 forward
        fwd_rs2_data=mem_wb_fwd_value;
    else begin 
        fwd_rs2_data=id_ex_rs2_data;
    end

 end
 endmodule
