// Omni-RISC APU — GPU Core: gpu_alu
//
// 4-lane SIMT integer ALU. One opcode, four lanes in lockstep.
// Operands are packed 128-bit vectors: lane0 = [31:0], lane1 = [63:32],
// lane2 = [95:64], lane3 = [127:96]. Same ALU-op encoding as the CPU:
//   0=ADD 1=SUB 2=AND 3=OR 4=XOR 5=SLT 6=SLTU 7=SLL 8=SRL 9=SRA 10=MUL(low)
module gpu_alu (
    input  [127:0] a,       // 4 lanes × 32 (lane0 in [31:0])
    input  [127:0] b,       // lane0 in [31:0]
    input  [3:0]   alu_op,
    output [127:0] r
);
    wire [31:0] a0 = a[31:0],  a1 = a[63:32],  a2 = a[95:64],  a3 = a[127:96];
    wire [31:0] b0 = b[31:0],  b1 = b[63:32],  b2 = b[95:64],  b3 = b[127:96];

    function [31:0] lane_op(input [31:0] x, input [31:0] y, input [3:0] op);
        case (op)
            4'd0:  lane_op = x + y;
            4'd1:  lane_op = x - y;
            4'd2:  lane_op = x & y;
            4'd3:  lane_op = x | y;
            4'd4:  lane_op = x ^ y;
            4'd5:  lane_op = {31'b0, ($signed(x) < $signed(y))};
            4'd6:  lane_op = {31'b0, (x < y)};
            4'd7:  lane_op = x << y[4:0];
            4'd8:  lane_op = x >> y[4:0];
            4'd9:  lane_op = $signed(x) >>> y[4:0];
            4'd10: lane_op = x * y;   // low 32 bits
            default: lane_op = 32'b0;
        endcase
    endfunction

    assign r[31:0]   = lane_op(a0, b0, alu_op);
    assign r[63:32]  = lane_op(a1, b1, alu_op);
    assign r[95:64]  = lane_op(a2, b2, alu_op);
    assign r[127:96] = lane_op(a3, b3, alu_op);
endmodule
