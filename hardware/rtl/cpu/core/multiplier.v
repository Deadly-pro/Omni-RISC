module multiplier(
  input  [31:0] operand_a,      // rs1 (already forwarded, from exec_stage)
  input  [31:0] operand_b,      // rs2 (already forwarded)
  input  [2:0]  funct3,         // selects MUL/MULH/MULHSU/MULHU
  output reg [31:0] result
);
/*
funct3   op        rs1 treated as   rs2 treated as   result
  000      MUL       (either)         (either)         product[31:0]
  001      MULH      signed           signed           product[63:32]
  010      MULHSU    signed           unsigned         product[63:32]
  011      MULHU     unsigned         unsigned         product[63:32]
*/
reg [63:0] a_ext,b_ext;
reg [63:0] product;
always @(*) begin
    case (funct3)
        3'b000:begin
            a_ext={{32{operand_a[31]}}, operand_a};
            b_ext={{32{operand_b[31]}}, operand_b};
            product=a_ext*b_ext;
            result=product[31:0];
        end
        3'b001:begin 
            a_ext={{32{operand_a[31]}}, operand_a};
            b_ext={{32{operand_b[31]}}, operand_b};
            product=a_ext*b_ext;
            result=product[63:32];
        end
        3'b010:begin 
            a_ext={{32{operand_a[31]}}, operand_a};
            b_ext={{32{1'b0}}, operand_b};
            product=a_ext*b_ext;
            result=product[63:32];
        end
        3'b011:begin 
            a_ext={{32{1'b0}}, operand_a};
            b_ext={{32{1'b0}}, operand_b};
            product=a_ext*b_ext;
            result=product[63:32];
        end
        default:begin 
            a_ext=64'h0000_0000_0000_0000;
            b_ext=64'h0000_0000_0000_0000;
            product=64'h0000_0000_0000_0000;
            result=32'h0000_0000;
        end   
    endcase
end
endmodule
