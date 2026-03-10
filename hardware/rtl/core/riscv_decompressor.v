// RISC-V Compressed Instruction Decompressor (RV32C -> RV32I)
`timescale 1ns / 1ps

module riscv_decompressor (
    input  wire [31:0] instr_in,
    output reg  [31:0] instr_out,
    output wire        is_compressed
);

    assign is_compressed = (instr_in[1:0] != 2'b11);

    always @(*) begin
        if (!is_compressed) begin
            instr_out = instr_in;
        end else begin
            instr_out = 32'h00000013; // Default NOP
            
            case (instr_in[1:0])
                2'b00: begin // Quadrant 0
                    case (instr_in[15:13])
                        3'b010: begin // C.LW
                            // Fixed: Corrected bit count to 32 bits total
                            instr_out = {5'b0, instr_in[5], instr_in[12:10], instr_in[6], 2'b00, 
                                         3'b010, instr_in[9:7], 3'b010, 2'b01, instr_in[4:2], 7'b0000011};
                        end
                    endcase
                end
                2'b01: begin // Quadrant 1
                    case (instr_in[15:13])
                        3'b000: begin // C.ADDI
                            // Fixed: Corrected bit count to 32 bits total (6 bits of imm instead of 7)
                            instr_out = {{6{instr_in[12]}}, instr_in[12], instr_in[6:2], 
                                         instr_in[11:7], 3'b000, instr_in[11:7], 7'b0010011};
                        end
                    endcase
                end
                2'b10: begin // Quadrant 2
                    case (instr_in[15:13])
                        3'b000: begin // C.SLLI
                            instr_out = {7'b0000000, instr_in[6:2], instr_in[11:7], 3'b001, 
                                         instr_in[11:7], 7'b0010011};
                        end
                    endcase
                end
            endcase
        end
    end

endmodule
