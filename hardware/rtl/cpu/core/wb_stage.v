module wb_stage (
      // ---- from the MEM/WB register ----
      input  [31:0] mem_wb_alu_result,
      input  [31:0] mem_wb_load_data,   // garbage today (32'b0), real after LSU
      input  [31:0] mem_wb_pc_plus4,
      input  [4:0]  mem_wb_rd,
      input         mem_wb_jump,
      input         mem_wb_mem_read,
      input         mem_wb_reg_write,

      // ---- to the regfile write port (loops back into decode_stage) ----
      output [4:0]  wb_rd_addr,
      output [31:0] wb_rd_data,
      output        wb_reg_write
  );
  assign wb_rd_addr=mem_wb_rd;
  assign wb_rd_data=mem_wb_jump ? mem_wb_pc_plus4 : (mem_wb_mem_read ? mem_wb_load_data : mem_wb_alu_result);
  assign wb_reg_write=mem_wb_reg_write;
  endmodule
