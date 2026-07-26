module lsu (
      // ---- from the EX/MEM bundle ----
      input  [31:0] addr,          // ex_mem_alu_result — rs1+imm, any alignment
      input  [31:0] store_data,    // ex_mem_store_data — rs2, LSB-justified
      input  [2:0]  funct3,        // loads:  000 LB 001 LH 010 LW 100 LBU 101 LHU
                                   // stores: 000 SB 001 SH 010 SW
      input         mem_write,

      // ---- to/from data_bram (word-oriented) ----
      output [31:0] dmem_addr,     // word-aligned: {addr[31:2], 2'b00}
      output [31:0] dmem_wdata,    // store_data shifted into the right byte lane(s)
      output reg [3:0]  dmem_wen     // per-byte write enable; 4'b0000 when !mem_write
);

  always @(*) begin
    case(funct3)
    3'b000:begin
       dmem_wen=mem_write?4'b0001<<addr[1:0]:4'b0000;
       end
    3'b001:begin
        dmem_wen=mem_write?(4'b0011<<addr[1:0]):4'b0000;
        end
    3'b010:begin
        dmem_wen=mem_write?4'b1111:4'b0000;
        end
    3'b100:begin 
        dmem_wen=mem_write?4'b0001<<addr[1:0]:4'b0000;
    end
    3'b101:begin
        dmem_wen=mem_write?4'b0011<<addr[1:0]:4'b0000;
        end
    default:begin 
        dmem_wen=4'b0000;
    end
    endcase 
  end
assign dmem_wdata=store_data<<(addr[1:0]*8);
assign dmem_addr={addr[31:2],2'b00};
endmodule
