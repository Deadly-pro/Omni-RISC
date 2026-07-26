// =============================================================================
// decoder.v — RV32IM Instruction Decoder (combinational)
// =============================================================================
// TODAY: code + test with `./hardware/sim/run_sim.sh cpu/tb_decoder`
//        (build imm_gen.v first — this module instantiates it)
//
// WHAT IT DOES
//   Cracks a raw 32-bit instruction into register addresses, immediate,
//   ALU operation, and control signals for every downstream stage.
//   Pure combinational — lives in ID stage.
//
// PORTS (must match tb_decoder.cpp exactly)
//   input  [31:0] instruction
//   output [4:0]  rs1, rs2, rd        // always instr[19:15], [24:20], [11:7]
//   output [31:0] immediate           // from imm_gen instance
//   output [3:0]  alu_op              // same encoding as alu.v (see tb_alu.cpp)
//   output [2:0]  funct3              // instr[14:12], passed to branch/mem logic
//   output [6:0]  funct7              // instr[31:25]
//   output        reg_write           // instruction writes rd
//   output        mem_read            // load
//   output        mem_write           // store
//   output        branch              // conditional branch (B-type)
//   output        jump                // JAL / JALR
//   output        is_mul_div          // R-type with funct7 == 7'b0000001 (M-ext)
//   output [1:0]  op_type             // 0=R, 1=I, 2=S, 3=B (per tb_decoder.cpp)
//   output        illegal_instr       // unrecognized opcode/funct combination
//
// OPCODE MAP (instr[6:0])
//   0110011 R-type ALU    | 0010011 I-type ALU   | 0000011 Load
//   0100011 Store         | 1100011 Branch       | 0110111 LUI
//   0010111 AUIPC         | 1101111 JAL          | 1100111 JALR
//   1110011 SYSTEM (CSR/ecall/ebreak — flag only; csr_file/trap_unit handle it)
//
// =============================================================================
module decoder(
  input  [31:0] instruction,
  output [4:0]  rs1, rs2, rd,
  output [31:0] immediate,           // from imm_gen instance
  output reg [3:0]  alu_op,              // same encoding as alu.v (see tb_alu.cpp)
  output [2:0]  funct3,              // instr[14:12], passed to branch/mem logic
  output [6:0]  funct7,              // instr[31:25]
  output reg reg_write,           // instruction writes rd
  output reg mem_read,            // load
  output reg mem_write,           // store
  output reg branch,              // conditional branch (B-type)
  output reg jump,                // JAL / JALR
  output reg is_mul_div,          // R-type with funct7 == 7'b0000001 (M-ext)
  output reg [1:0] op_type,             // 0=R, 1=I, 2=S, 3=B (per tb_decoder.cpp)
  output reg illegal_instr       // unrecognized opcode/funct combination
);
assign rs1=instruction[19:15];
assign rs2=instruction[24:20];
assign rd=instruction[11:7];
assign funct3=instruction[14:12];
assign funct7=instruction[31:25];
reg [2:0] imm_type;

always @(*)begin
    imm_type=0;
    reg_write=0;
    mem_read=0;
    mem_write=0;
    branch=0;
    jump=0;
    op_type=0;
    alu_op=0;
    is_mul_div=0;
    illegal_instr=0;
    case(instruction[6:0])
    7'b0110011:begin //R Type
                reg_write=1; // write enable for rd
                op_type=0; // type R
                imm_type=5;//invaid type returns 0
                if(funct7==7'b0000000)begin
                   case(funct3)
                   3'b000:alu_op=0; //ADD
                   3'b001:alu_op=7; //SLL
                   3'b010:alu_op=5;//SLT
                   3'b011:alu_op=6;//SLTU
                   3'b100:alu_op=4;//XOR
                   3'b101:alu_op=8;//SRL
                   3'b110:alu_op=3;//OR
                   3'b111:alu_op=2;//AND
                   default:;
               endcase
               end
               else if (funct7==7'b0100000)begin
                   case(funct3)
                    3'b000:alu_op=1;
                    3'b101:alu_op=9;
                    default: ;
                   endcase
               end
               else if(funct7==7'b0000001)is_mul_div=1;
    end
    7'b0010011:begin //I Type
        reg_write=1; //write enable for rd
        op_type=1;
        imm_type=0;
        case(funct3)
        3'b001:alu_op=7;//SLLI
        3'b101:if(funct7==7'b0100000)alu_op=9;//SRAI
                else if (funct7==7'b0000000)alu_op=8;//SRLI
        3'b000:alu_op=0;//ADDI
        3'b010:alu_op=5;//SLTI
        3'b011:alu_op=6;//SLTUI
        3'b100:alu_op=4;//XORI
        3'b110:alu_op=3;//ORI
        3'b111:alu_op=2;//ANDI
        endcase
    end
    7'b0000011:begin //Load
                mem_read=1; // read memory
                reg_write=1; //write to rd
                op_type=1; //Load uses Immediate for addresses
                case(funct3)
                3'b000:;//LB
                3'b001:;//LH
                3'b010:;//LW
                3'b100:;//LBU
                3'b101:;//LHU
                default:;
                endcase
    end
    7'b0100011:begin //Store
        mem_write=1;
        op_type=2;
        imm_type=1;
        case(funct3)
        3'b000:;//SB
        3'b001:;//SH
        3'b010:;//SW
        default:;
        endcase
    end
    7'b1100011:begin
        branch=1; //Branch
        imm_type=2;
        op_type=3;
    end
    7'b0110111:begin //LUI
        reg_write=1;
        alu_op=10;
        imm_type=3;
    end
    7'b0010111:begin //AUIPC
        reg_write=1;
        alu_op=11;
        imm_type=3;
    end
    7'b1101111:begin //JAL
        jump=1;
        reg_write=1;
        imm_type=4;
    end
    7'b1100111:begin //JALR
        jump=1;
        reg_write=1;
        alu_op=0;
        op_type=1;
    end
    7'b1110011:begin
        if(funct3!=0) reg_write=1;
        else reg_write=0;
    end
    default:illegal_instr=1;
    endcase
end
imm_gen imm1(.instruction(instruction),.imm_type(imm_type),.immediate(immediate));
endmodule
