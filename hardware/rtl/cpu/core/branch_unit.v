// =============================================================================
// branch_unit.v — Branch Condition Resolution (combinational)
// =============================================================================
// STRETCH for today (no TB yet — simple enough for a table-driven tb_alu-style
//                    testbench if you want one)
//
// WHAT IT DOES
//   Decides taken/not-taken for conditional branches in EX stage. With static
//   not-taken prediction, asserts redirect only when actually taken (branches)
//   or always (JAL/JALR).
//
// SUGGESTED PORTS
//   input  [31:0] rs1_data         // post-forwarding values!
//   input  [31:0] rs2_data
//   input  [2:0]  funct3           // which comparison
//   input         is_branch        // from decoder
//   input         is_jump          // JAL/JALR — unconditionally taken
//   output        take_branch      // → pc_gen.redirect_valid, hazard_unit flush
//
// FUNCT3 MAP (B-type)
//   000 BEQ  (rs1 == rs2)          001 BNE  (rs1 != rs2)
//   100 BLT  (signed <)            101 BGE  (signed >=)
//   110 BLTU (unsigned <)          111 BGEU (unsigned >=)
//
// GOTCHAS
//   - BLT/BGE need $signed() on both operands; BLTU/BGEU plain
//   - take_branch = is_jump || (is_branch && condition)
//   - Inputs must come from the forwarding muxes, NOT raw ID/EX register
//     values, or "add then beq" back-to-back sequences compare stale data
//
// DONE WHEN: all 6 comparisons correct incl. signed/unsigned disagreement
//            (e.g. -1 < 1 signed, but 0xFFFFFFFF > 1 unsigned).
// =============================================================================
module branch_unit(
  input  [31:0] rs1_data,         // post-forwarding values!
  input  [31:0] rs2_data,
  input  [2:0]  funct3,           // which comparison
  input         is_branch,        // from decoder
  input         is_jump,          // JAL/JALR — unconditionally taken
  output reg take_branch      // → pc_gen.redirect_valid, hazard_unit flush
);
always @(*) begin
    take_branch=0;
    if(is_jump)take_branch=1;
    else if(is_branch)begin
    case(funct3)
    3'b000:if(rs1_data==rs2_data)take_branch=1; //BEQ
    3'b001:if(rs1_data!=rs2_data)take_branch=1; //BNE
    3'b100:if($signed(rs1_data)<$signed(rs2_data))take_branch=1; //BLT
    3'b101:if($signed(rs1_data)>=$signed(rs2_data))take_branch=1; //BGE
    3'b110:if(rs1_data<rs2_data)take_branch=1; //BLTU
    3'b111:if(rs1_data>=rs2_data)take_branch=1; //BGEU
    default:take_branch=0;
    endcase
    end
end
endmodule
