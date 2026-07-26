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
      output [31:0] mem_wb_rdata, 
      output reg [4:0]  mem_wb_rd,
      output reg [31:0] mem_wb_pc_plus4,
      output reg        mem_wb_jump,
      output reg        mem_wb_mem_read,    // WB's mux select for load_data
      output reg        mem_wb_reg_write,
      output reg [1:0] mem_wb_ld_lsb,
      output reg [2:0] mem_wb_ld_funct3
  );
  wire [31:0] dmem_addr, dmem_wdata, dmem_rdata;
  wire [3:0]  dmem_wen;
  
always @(posedge clk)begin
if (reset)begin
    mem_wb_alu_result <= 32'b0;
    mem_wb_rd <= 5'b0;
    mem_wb_pc_plus4 <= 32'b0;
    mem_wb_jump <= 1'b0;
    mem_wb_mem_read <= 1'b0;
    mem_wb_reg_write <= 1'b0;
    mem_wb_ld_lsb    <= 2'b00;
    mem_wb_ld_funct3 <= 3'b000;
    end 
else if (!stall) begin
    mem_wb_alu_result <= ex_mem_alu_result;
    mem_wb_rd <= ex_mem_rd;
    mem_wb_pc_plus4 <= ex_mem_pc_plus4;
    mem_wb_jump <= ex_mem_jump;
    mem_wb_mem_read <= ex_mem_mem_read;
    mem_wb_reg_write <= ex_mem_reg_write;
    mem_wb_ld_lsb    <= ex_mem_alu_result[1:0];
    mem_wb_ld_funct3 <= ex_mem_funct3;
end
end

lsu u_lsu(
      .addr(ex_mem_alu_result), .store_data(ex_mem_store_data),
      .funct3(ex_mem_funct3), .mem_write(ex_mem_mem_write),
      .dmem_addr(dmem_addr), .dmem_wdata(dmem_wdata), .dmem_wen(dmem_wen)
  );
  data_bram u_dbram(
      .clk(clk), .addr(dmem_addr), .wdata(dmem_wdata),
      .wen(dmem_wen), .rdata(mem_wb_rdata)
  );
endmodule
