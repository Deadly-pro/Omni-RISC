// Omni-RISC APU — CPU: wb_stage
//
// Writeback: selects the register-write value (ALU result, load data after
// the sub-word align/extend done in MEM, or pc+4 for JAL/JALR) and routes
// it with the destination address to the regfile write port. Pure
// combinational — no state.
module wb_stage (
      // ---- from the MEM/WB register ----
      input  [31:0] mem_wb_alu_result,
      input  [31:0] mem_wb_pc_plus4,
      input  [31:0] mem_wb_rdata,
      input  [4:0]  mem_wb_rd,
      input         mem_wb_jump,
      input         mem_wb_mem_read,
      input         mem_wb_reg_write,
      input  [1:0]  mem_wb_ld_lsb,
      input  [2:0]  mem_wb_ld_funct3,  
      // ---- to the regfile write port (loops back into decode_stage) ----
      output [4:0]  wb_rd_addr,
      output [31:0] wb_rd_data,
      output        wb_reg_write
  );
  reg [31:0] shifted,load_data;
  always @(*) begin
    shifted=mem_wb_rdata>>(mem_wb_ld_lsb*8);
    case(mem_wb_ld_funct3)
    3'b000:begin
       load_data={{24{shifted[7]}},shifted[7:0]};
       end
    3'b001:begin
        load_data={{16{shifted[15]}},shifted[15:0]};
        end
    3'b010:begin
        load_data=shifted;
        end
    3'b100:begin 
        load_data={{24{1'b0}},shifted[7:0]};
    end
    3'b101:begin
        load_data={{16{1'b0}},shifted[15:0]};
        end
    default:begin 
        load_data=32'h0000_0000;
    end
    endcase 
  end

  assign wb_rd_addr=mem_wb_rd;
  assign wb_rd_data=mem_wb_jump ? mem_wb_pc_plus4 : (mem_wb_mem_read ? load_data : mem_wb_alu_result);
  assign wb_reg_write=mem_wb_reg_write;
  
  endmodule
