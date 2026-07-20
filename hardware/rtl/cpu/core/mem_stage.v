module mem_stage (
      input         clk,
      input         reset,
      input         stall,

      // ---- EX/MEM bundle in (matches exec_stage outputs 1:1) ----
      input  [31:0] ex_mem_alu_result,
      input  [31:0] ex_mem_store_data,   // unused today — consumed here when LSU lands
      input  [4:0]  ex_mem_rd,
      input  [2:0]  ex_mem_funct3,       // unused today — load/store size, later
      input  [31:0] ex_mem_pc_plus4,
      input         ex_mem_jump,
      input         ex_mem_reg_write,
      input         ex_mem_mem_read,
      input         ex_mem_mem_write,    // unused today

      // ---- MEM/WB bundle out ----
      output reg [31:0] mem_wb_alu_result,
      output reg [31:0] mem_wb_load_data,   // placeholder: register 32'b0 for now
      output reg [4:0]  mem_wb_rd,
      output reg [31:0] mem_wb_pc_plus4,
      output reg        mem_wb_jump,
      output reg        mem_wb_mem_read,    // WB's mux select for load_data
      output reg        mem_wb_reg_write
  );
always @(posedge clk)begin
if (reset)begin
    mem_wb_alu_result <= 32'b0;
    mem_wb_load_data <= 32'b0;
    mem_wb_rd <= 5'b0;
    mem_wb_pc_plus4 <= 32'b0;
    mem_wb_jump <= 1'b0;
    mem_wb_mem_read <= 1'b0;
    mem_wb_reg_write <= 1'b0;
    end 
else if (!stall) begin
    mem_wb_alu_result <= ex_mem_alu_result;
    mem_wb_load_data <= 32'b0; 
    mem_wb_rd <= ex_mem_rd;
    mem_wb_pc_plus4 <= ex_mem_pc_plus4;
    mem_wb_jump <= ex_mem_jump;
    mem_wb_mem_read <= ex_mem_mem_read;
    mem_wb_reg_write <= ex_mem_reg_write;
end
end
endmodule
