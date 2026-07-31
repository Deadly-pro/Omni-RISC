// =============================================================================
// pc_gen.v — Program Counter Generation (sequential)
// =============================================================================
// STRETCH for today (no TB yet — write tb_pc_gen.cpp when you get here;
//                    copy the tb_regfile.cpp clocking pattern)
//
// WHAT IT DOES
//   Holds the PC register and selects the next PC each cycle.
//   Priority: reset > trap/mret target > branch/jump redirect > stall hold > PC+4
//
// SUGGESTED PORTS
//   input         clk
//   input         reset            // PC ← RESET_VECTOR (32'h0000_0000)
//   input         stall            // hold PC (from hazard_unit)
//   input         redirect_valid   // branch taken / jump resolved in EX
//   input  [31:0] redirect_target
//   input         trap_valid       // from trap_unit (higher priority)
//   input  [31:0] trap_target      // mtvec (trap) or mepc (mret)
//   output reg [31:0] pc           // current fetch address
//   output [31:0] pc_plus4         // pc + 4 (link value for JAL/JALR)
//
// GOTCHAS
//   - Priority order matters: get it wrong and a branch arriving during a
//     stall corrupts the fetch stream
//   - Redirect during stall: redirect WINS (the stalled instr is being flushed)
//
// DONE WHEN: sequential fetch counts 0,4,8...; redirect jumps correctly;
//            stall holds; reset returns to vector.
// =============================================================================
module pc_gen(
  input  clk,
  input  reset,            // PC ← RESET_VECTOR (32'h0000_0000)
  input  stall,            // hold PC (from hazard_unit)
  input  redirect_valid,   // branch taken / jump resolved in EX
  input  [31:0] redirect_target,
  input  trap_valid,       // from trap_unit (higher priority)
  input  [31:0] trap_target,      // mtvec (trap) or mepc (mret)
  output reg [31:0] pc /* verilator public */,   // current fetch address (public for the compliance harness)
  output reg [31:0] pc_plus4         // pc + 4 (link value for JAL/JALR)
);
always @(posedge clk) begin
    if(reset)begin
        pc<=32'h0000_0000;
    end
    else if(trap_valid)begin
	pc<=trap_target;
    end
    else if(redirect_valid)begin
        pc<=redirect_target;
    end
    else if(stall)pc<=pc;
    else begin
        pc<=pc+4;
    end
end
assign pc_plus4=pc+4;
endmodule
