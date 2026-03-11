// Instruction decode stage integration for Omni-RISC.
`timescale 1ns / 1ps

module ins_decode (
    input  wire        clk,
    input  wire        rst,
    input  wire [31:0] id_instruction_in,
    input  wire [31:0] id_pc_plus_4_in,
    input  wire [31:0] id_pc_in,
    input  wire [4:0]  ex_rd_addr_in,
    input  wire        ex_mem_read_in,
    input  wire        ex_reg_write_in,
    input  wire [4:0]  wb_write_addr_in,
    input  wire [31:0] wb_write_data_in,
    input  wire        wb_reg_write_en_in,
    output wire        pipeline_stall_out,
    output wire [31:0] id_pc_plus_4_out,
    output wire [31:0] id_pc_out,
    output wire [31:0] id_read_data1_out,
    output wire [31:0] id_read_data2_out,
    output wire [31:0] id_immediate_out,
    output wire [4:0]  id_rs1_addr_out,
    output wire [4:0]  id_rs2_addr_out,
    output wire [4:0]  id_rd_addr_out,
    output wire [31:0] id_instruction_out,
    output wire        id_mem_read_out,
    output wire        id_mem_write_out,
    output wire        id_reg_write_out,
    output wire        id_mem_to_reg_out,
    output wire        id_alu_src_out,
    output wire        id_branch_out,
    output wire        id_jump_out,
    output wire [3:0]  id_alu_ctrl_out,
    output wire        id_write_from_pc_out
);

    assign id_instruction_out = id_instruction_in;
    assign id_pc_plus_4_out   = id_pc_plus_4_in;
    assign id_pc_out          = id_pc_in;

    assign id_rs1_addr_out = id_instruction_in[19:15];
    assign id_rs2_addr_out = id_instruction_in[24:20];
    assign id_rd_addr_out  = id_instruction_in[11:7];

    hazard_unit hu (
        .id_rs1_addr(id_rs1_addr_out),
        .id_rs2_addr(id_rs2_addr_out),
        .ex_rd_addr(ex_rd_addr_in),
        .ex_mem_read(ex_mem_read_in),
        .pipeline_stall(pipeline_stall_out)
    );

    reg_file rf (
        .clk(clk),
        .rst(rst),
        .read_addr1(id_rs1_addr_out),
        .read_addr2(id_rs2_addr_out),
        .write_addr(wb_write_addr_in),
        .write_data(wb_write_data_in),
        .write_en(wb_reg_write_en_in),
        .read_data1(id_read_data1_out),
        .read_data2(id_read_data2_out)
    );

    control_unit cu (
        .opcode(id_instruction_in[6:0]),
        .funct3(id_instruction_in[14:12]),
        .funct7(id_instruction_in[31:25]),
        .RegWrite(id_reg_write_out),
        .MemToReg(id_mem_to_reg_out),
        .MemRead(id_mem_read_out),
        .MemWrite(id_mem_write_out),
        .ALUSrc(id_alu_src_out),
        .ALUCtrl(id_alu_ctrl_out),
        .Branch(id_branch_out),
        .Jump(id_jump_out),
        .WriteFromPC(id_write_from_pc_out)
    );

    imm_gen ig (
        .instruction(id_instruction_in),
        .immediate(id_immediate_out)
    );

endmodule

module control_unit(
    input  wire [6:0] opcode,
    input  wire [2:0] funct3,
    input  wire [6:0] funct7,
    output reg        RegWrite,
    output reg        MemToReg,
    output reg        MemRead,
    output reg        MemWrite,
    output reg        ALUSrc,
    output reg [3:0]  ALUCtrl,
    output reg        Branch,
    output reg        Jump,
    output reg        WriteFromPC
);
    always @(*) begin
        RegWrite     = 0;
        MemToReg     = 0;
        MemRead      = 0;
        MemWrite     = 0;
        ALUSrc       = 0;
        Branch       = 0;
        ALUCtrl      = 4'b0000;
        WriteFromPC  = 0;
        Jump         = 0;

        case (opcode)
            7'b0110011: begin // R-type
                RegWrite = 1;
                case (funct3)
                    3'b000: ALUCtrl = (funct7[5]) ? 4'b0001 : 4'b0000; // SUB : ADD
                    3'b001: ALUCtrl = 4'b0110; // SLL
                    3'b010: ALUCtrl = 4'b0111; // SLT
                    3'b011: ALUCtrl = 4'b1000; // SLTU
                    3'b100: ALUCtrl = 4'b0100; // XOR
                    3'b101: ALUCtrl = (funct7[5]) ? 4'b1010 : 4'b1001; // SRA : SRL
                    3'b110: ALUCtrl = 4'b0011; // OR
                    3'b111: ALUCtrl = 4'b0010; // AND
                    default: ALUCtrl = 4'b0000;
                endcase
            end
            7'b0010011: begin // I-type
                RegWrite = 1;
                ALUSrc   = 1;
                case (funct3)
                    3'b000: ALUCtrl = 4'b0000; // ADDI
                    3'b010: ALUCtrl = 4'b0111; // SLTI
                    3'b011: ALUCtrl = 4'b1000; // SLTIU
                    3'b100: ALUCtrl = 4'b0100; // XORI
                    3'b110: ALUCtrl = 4'b0011; // ORI
                    3'b111: ALUCtrl = 4'b0010; // ANDI
                    3'b001: ALUCtrl = 4'b0110; // SLLI
                    3'b101: ALUCtrl = (funct7[5]) ? 4'b1010 : 4'b1001; // SRAI : SRLI
                    default: ALUCtrl = 4'b0000;
                endcase
            end
            7'b0000011: begin // Load (LW)
                RegWrite = 1;
                MemToReg = 1;
                MemRead  = 1;
                ALUSrc   = 1;
                ALUCtrl  = 4'b0000;
            end
            7'b0100011: begin // Store (SW)
                MemWrite = 1;
                ALUSrc   = 1;
                ALUCtrl  = 4'b0000;
            end
            7'b1100011: begin // Branch
                Branch   = 1;
                ALUCtrl  = 4'b0001;
            end
            7'b1101111: begin // JAL
                RegWrite     = 1;
                Jump         = 1;
                WriteFromPC  = 1;
            end
            7'b1100111: begin // JALR
                RegWrite     = 1;
                Jump         = 1;
                ALUSrc       = 1;
                WriteFromPC  = 1;
            end
            7'b0110111: begin // LUI
                RegWrite = 1;
                ALUSrc   = 1;
                ALUCtrl  = 4'b0101;
            end
            7'b0010111: begin // AUIPC
                RegWrite     = 1;
                ALUSrc       = 1;
                WriteFromPC  = 1;
                ALUCtrl      = 4'b0000;
            end
            default: ;
        endcase
    end
endmodule

module reg_file (
    input  wire        clk,
    input  wire        rst,
    input  wire [4:0]  read_addr1,
    input  wire [4:0]  read_addr2,
    input  wire [4:0]  write_addr,
    input  wire [31:0] write_data,
    input  wire        write_en,
    output wire [31:0] read_data1,
    output wire [31:0] read_data2
);
    reg [31:0] registers [31:0];

    assign read_data1 = (read_addr1 == 5'b0) ? 32'b0 : registers[read_addr1];
    assign read_data2 = (read_addr2 == 5'b0) ? 32'b0 : registers[read_addr2];

    integer i;
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            for (i = 0; i < 32; i = i + 1)
                registers[i] <= 32'b0;
        end else if (write_en && (write_addr != 5'b0)) begin
            registers[write_addr] <= write_data;
        end
    end
endmodule

module imm_gen (
    input  wire [31:0] instruction,
    output reg  [31:0] immediate
);
    always @(*) begin
        case (instruction[6:0])
            7'b0110011: immediate = 32'b0; // R-type
            7'b0010011: immediate = {{20{instruction[31]}}, instruction[31:20]}; // I-type
            7'b0000011: immediate = {{20{instruction[31]}}, instruction[31:20]}; // I-type (Load)
            7'b1100111: immediate = {{20{instruction[31]}}, instruction[31:20]}; // I-type (JALR)
            7'b0100011: immediate = {{20{instruction[31]}}, instruction[31:25], instruction[11:7]}; // S-type
            7'b1100011: immediate = {{20{instruction[31]}}, instruction[7], instruction[30:25], instruction[11:8], 1'b0}; // B-type
            7'b0110111: immediate = {instruction[31:12], 12'b0}; // U-type (LUI)
            7'b0010111: immediate = {instruction[31:12], 12'b0}; // U-type (AUIPC)
            7'b1101111: immediate = {{12{instruction[31]}}, instruction[19:12], instruction[20], instruction[30:21], 1'b0}; // J-type
            default: immediate = 32'b0;
        endcase
    end
endmodule

module hazard_unit (
    input  wire [4:0] id_rs1_addr,
    input  wire [4:0] id_rs2_addr,
    input  wire [4:0] ex_rd_addr,
    input  wire       ex_mem_read,
    output reg        pipeline_stall
);
    always @(*) begin
        if (ex_mem_read && (ex_rd_addr != 5'b0) && ((ex_rd_addr == id_rs1_addr) || (ex_rd_addr == id_rs2_addr)))
            pipeline_stall = 1'b1;
        else
            pipeline_stall = 1'b0;
    end
endmodule

module if_id_buffer (
    input  wire        clk,
    input  wire        rst,
    input  wire        pipeline_stall,
    input  wire        fetch_stall,
    input  wire [31:0] if_instruction_in,
    input  wire [31:0] if_pc_plus_4_in,
    input  wire [31:0] if_pc_in,
    output reg  [31:0] id_instruction_out,
    output reg  [31:0] id_pc_plus_4_out,
    output reg  [31:0] id_pc_out
);
    always @(posedge clk or posedge rst) begin
        if (rst) begin
            id_instruction_out <= 32'h00000013;
            id_pc_plus_4_out   <= 32'b0;
            id_pc_out          <= 32'b0;
        end else if (!pipeline_stall && !fetch_stall) begin
            id_instruction_out <= if_instruction_in;
            id_pc_plus_4_out   <= if_pc_plus_4_in;
            id_pc_out          <= if_pc_in;
        end
    end
endmodule
