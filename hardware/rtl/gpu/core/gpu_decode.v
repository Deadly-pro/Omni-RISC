// Omni-RISC APU — GPU Core: gpu_decode
//
// Decodes 16-bit GPU instructions into control signals for exec_lane.
// Instruction format (16-bit):
//   [15:12] opcode
//   [11:9]  rd
//   [8:6]   rs1
//   [5:3]   rs2
//   [2:0]   funct3
//
// Opcode map:
//   0 = ALU  : alu_op = funct3        (0=ADD 1=SUB 2=AND 3=OR 4=XOR 5=SLT 6=SLTU 7=SLL)
//   1 = LSU  : funct3[0]=0 load  rd <- sp[rs1]
//              funct3[0]=1 store sp[rs1] <- rs2
//   2 = BR   : unconditional, PC <- branch_target (byte addr, instr[7:0])
//   3 = MUL  : alu_op = 10 (legacy alias)
//   4 = ALU2 : alu_op = 8 + funct3    (0=SRL 1=SRA 2=MUL)
//   5 = LDI  : rd <- signext(instr[8:0]) broadcast to all lanes
//   F = HALT : warp done
module gpu_decode (
    input  [15:0] instr,
    output [3:0]  alu_op,
    output [2:0]  rd_addr,
    output [2:0]  rs1_addr,
    output [2:0]  rs2_addr,
    output        reg_write,
    output        mem_read,
    output        mem_write,
    output        is_ldi,
    output [31:0] ldi_imm,      // sign-extended imm9, broadcast at warp level
    output        is_halt,
    output        is_branch,
    output [7:0]  branch_target
);

    wire [3:0] opcode = instr[15:12];

    localparam OP_ALU  = 4'h0;
    localparam OP_LSU  = 4'h1;
    localparam OP_BR   = 4'h2;
    localparam OP_MUL  = 4'h3;
    localparam OP_ALU2 = 4'h4;
    localparam OP_LDI  = 4'h5;
    localparam OP_HALT = 4'hF;

    assign alu_op    = (opcode == OP_ALU)  ? {1'b0, instr[2:0]} :
                       (opcode == OP_ALU2) ? {1'b1, instr[2:0]} :
                       (opcode == OP_MUL)  ? 4'd10 :
                       4'd0;

    assign rd_addr   = instr[11:9];
    assign rs1_addr  = instr[8:6];
    assign rs2_addr  = instr[5:3];
    assign branch_target = instr[7:0];

    assign mem_read  = (opcode == OP_LSU) && ~instr[0];
    assign mem_write = (opcode == OP_LSU) &&  instr[0];
    assign is_ldi    = (opcode == OP_LDI);
    assign ldi_imm   = {{23{instr[8]}}, instr[8:0]};
    assign is_halt   = (opcode == OP_HALT);
    assign is_branch = (opcode == OP_BR);

    assign reg_write = (opcode == OP_ALU) || (opcode == OP_ALU2) ||
                       (opcode == OP_MUL) || (opcode == OP_LDI)  || mem_read;

endmodule
