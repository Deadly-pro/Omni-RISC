// =============================================================================
// regfile.v — Architectural Register File (32 × 32-bit)
// =============================================================================
// TODAY: code + test with `./hardware/sim/run_sim.sh cpu/tb_regfile`
//
// WHAT IT DOES
//   32 general-purpose registers. Read in ID stage (combinational, so decode
//   can use rs1/rs2 the same cycle), written in WB stage (clocked).
//   Async read → synthesizes to LUT/distributed RAM on Artix-7 (correct).
//
// PORTS
//   input         clk
//   input         reset          // synchronous: zero all registers
//   input  [4:0]  rs1_addr
//   input  [4:0]  rs2_addr
//   input  [4:0]  rd_addr
//   input  [31:0] rd_data
//   input         rd_write_en    // write rd_data → x[rd_addr] on posedge
//   output [31:0] rs1_data       // combinational (async) read
//   output [31:0] rs2_data       // combinational (async) read
//
// GOTCHAS
//   - x0 is ALWAYS zero: gate it in the READ path (assign rs1_data =
//     (rs1_addr == 0) ? 32'b0 : mem[rs1_addr]) — don't rely on never writing it
//   - Write-then-read same cycle: with async read + sync write, a read of the
//     register being written this cycle returns the OLD value — expected for
//     this design (forwarding_net handles that hazard at pipeline level).
//
// DONE WHEN: tb_regfile passes (x0 writes ignored, all 31 regs hold values,
//            both read ports independent).
// =============================================================================
module regfile(
  input clk,
  input reset,          // synchronous: zero all registers
  input  [4:0]  rs1_addr,
  input  [4:0]  rs2_addr,
  input  [4:0]  rd_addr,
  input  [31:0] rd_data,
  input rd_write_en,    // write rd_data → x[rd_addr] on posedge
  output [31:0] rs1_data,       // combinational (async) read
  output [31:0] rs2_data       // combinational (async) read
);
reg [31:0] reg_file[0:31]/* verilator public */; //32 registers of 32 bit data
integer i;
always @(posedge clk) begin
    reg_file[0]<=32'b0;
    if (rd_write_en)begin
        if(rd_addr!=0)begin
        reg_file[rd_addr]<=rd_data;
        end
    end

    if (reset) begin
        for(i=0;i<32;i=i+1)begin
            reg_file[i]<=32'b0;
        end
    end
end
assign rs1_data=(rs1_addr==0)?32'h0000_0000:(rd_write_en && (rs1_addr==rd_addr)?rd_data:reg_file[rs1_addr]);
assign rs2_data=(rs2_addr==0)?32'h0000_0000:(rd_write_en && (rs2_addr==rd_addr)?rd_data:reg_file[rs2_addr]);
endmodule
