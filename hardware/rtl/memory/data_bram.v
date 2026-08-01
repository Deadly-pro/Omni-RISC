 module data_bram #(
    parameter MemFile = ""            // empty = zeroed (bare-CPU tests); SoC sets the firmware image
 ) (
      input             clk,
      input      [31:0] addr,      // word-aligned, from the LSU
      input      [31:0] wdata,
      input      [3:0]  wen,
      output [31:0] rdata          // combinational read; mem_stage registers it
  );
  reg [31:0] mem [0:65535] /* verilator public */; // 256KB — sized for riscv-arch-test
  initial begin
    if (MemFile != "") $readmemh(MemFile, mem);   // SoC firmware data image (word-indexed)
  end
  always @(posedge clk) begin
    if(wen[0])mem[addr[17:2]][7:0]<=wdata[7:0];
    if(wen[1])mem[addr[17:2]][15:8]<=wdata[15:8];
    if(wen[2])mem[addr[17:2]][23:16]<=wdata[23:16];
    if(wen[3])mem[addr[17:2]][31:24]<=wdata[31:24];
  end
  assign rdata = mem[addr[17:2]];
  endmodule
