// =============================================================================
// imm_gen.v — Immediate Generator (combinational)
// =============================================================================
// TODAY: code this BEFORE decoder.v — decoder instantiates it.
//        No standalone TB; it's tested through tb_decoder's immediate checks.
//
// WHAT IT DOES
//   Extracts and sign-extends the immediate from a raw 32-bit instruction,
//   according to format. All five variants reshuffle bits of instr[31:7];
//   bit 31 is ALWAYS the sign bit.
//
// PORTS
//   input  [31:0] instruction
//   input  [2:0]  imm_type       // 0=I, 1=S, 2=B, 3=U, 4=J
//   output [31:0] immediate      // sign-extended (U-type: low 12 bits zero)
//
// THE FIVE FORMATS (RISC-V spec Fig 2.4; i = instruction)
//   I: {{20{i[31]}}, i[31:20]}                                  loads, ALU-imm, JALR
//   S: {{20{i[31]}}, i[31:25], i[11:7]}                         stores
//   B: {{19{i[31]}}, i[31], i[7], i[30:25], i[11:8], 1'b0}      branches (×2, even)
//   U: {i[31:12], 12'b0}                                        LUI, AUIPC
//   J: {{11{i[31]}}, i[31], i[19:12], i[20], i[30:21], 1'b0}    JAL (×2, even)
//
// GOTCHAS
//   - B and J immediates have an implicit low zero bit (targets are 2-aligned)
//   - B-type bit order is scrambled: imm[12|10:5] in i[31:25], imm[4:1|11] in i[11:7]
//   - One wrong bit here = branches jump to garbage. Hand-verify each format
//     against https://luplab.gitlab.io/rvcodecjs/ with 2-3 examples
//
// DONE WHEN: tb_decoder's immediate checks pass for I/S/B/U/J instructions
//            including negative immediates (sign extension).
// =============================================================================
module imm_gen(
  input  [31:0] instruction,
  input  [2:0]  imm_type,       // 0=I, 1=S, 2=B, 3=U, 4=J
  output reg [31:0] immediate      // sign-extended (U-type: low 12 bits zero)
);
always @(*)begin
case(imm_type)
    3'b000:immediate={{20{instruction[31]}}, instruction[31:20]};
    3'b001:immediate={{20{instruction[31]}}, instruction[31:25],instruction[11:7]};
    3'b010:immediate={{19{instruction[31]}}, instruction[31], instruction[7], instruction[30:25], instruction[11:8], 1'b0};
    3'b011:immediate={instruction[31:12], 12'b0};
    3'b100:immediate={{11{instruction[31]}}, instruction[31], instruction[19:12], instruction[20], instruction[30:21], 1'b0};
    default:immediate=32'b0;
    endcase
end
endmodule
