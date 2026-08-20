// =============================================================================
// alu.v — Arithmetic Logic Unit (combinational, 0-cycle)
// =============================================================================
// TODAY: code + test with `./hardware/sim/run_sim.sh cpu/tb_alu`
//
// WHAT IT DOES
//   Pure combinational ALU. No clock, no reset. Result must settle in one
//   cycle — it sits in the EX stage between the ID/EX and EX/MEM registers.
//
// PORTS
//   input  [31:0] operand_a     // rs1 value (or PC for AUIPC)
//   input  [31:0] operand_b     // rs2 value or immediate (mux'd in exec_stage)
//   input  [3:0]  alu_op        // operation select — encoding MUST match tb_alu.cpp:
//                               //   0=ADD  1=SUB  2=AND  3=OR   4=XOR
//                               //   5=SLT  6=SLTU 7=SLL  8=SRL  9=SRA
//                               //   10=LUI_PASS (result = operand_b)
//                               //   11=AUIPC    (result = a + b, same as ADD)
    //                               //   12=ATOMIC_PASS (result = operand_a for LR; pass-through for SC)
//   output [31:0] result
//   output        zero_flag     // (result == 32'b0) — used by branch logic
//
// =============================================================================
module alu (
    input [31:0] operand_a,
    input [31:0] operand_b,
    input [3:0] alu_op,
    output reg [31:0] result,
    output reg zero_flag
);
always @(*) begin
result=32'b0;
zero_flag=1'b0;
case(alu_op)
    4'b0000:result=operand_a+operand_b; //add
    4'b0001:result=operand_a-operand_b; //subs
    4'b0010:result=operand_a&operand_b; //and
    4'b0011:result=operand_a|operand_b; //or
    4'b0100:result=operand_a^operand_b; //xor
    4'b0101:result={31'b0,{$signed(operand_a)<$signed(operand_b)}}; //SLT
    4'b0110:result={31'b0,{operand_a<operand_b}};//SLTU
    4'b0111:result=operand_a<<operand_b[4:0];//SLL
    4'b1000:result=operand_a>>operand_b[4:0];//SRL
    4'b1001:result=$signed(operand_a)>>>operand_b[4:0]; //SRA
    4'b1010:result=operand_b;//LUI_PASS
    4'b1011:result=operand_a+operand_b;//AUIPC
    4'b1100:result=operand_a;//ATOMIC_PASS (LR returns loaded value, SC passes through)
    default:begin
            result=32'b0;
            zero_flag=1'b1;
            end
endcase
if (result==32'b0)begin
    zero_flag=1'b1;
end

end
endmodule
