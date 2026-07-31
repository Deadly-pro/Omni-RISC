 module data_bram (
      input             clk,
      input      [31:0] addr,      // word-aligned, from the LSU
      input      [31:0] wdata,
      input      [3:0]  wen,
      output reg [31:0] rdata      // registered read, 1-cycle latency
  );
  reg [31:0] mem [0:65535] /* verilator public */; // 256KB — sized for riscv-arch-test
  always @(posedge clk) begin
    if(wen[0])mem[addr[17:2]][7:0]<=wdata[7:0];
    if(wen[1])mem[addr[17:2]][15:8]<=wdata[15:8];
    if(wen[2])mem[addr[17:2]][23:16]<=wdata[23:16];
    if(wen[3])mem[addr[17:2]][31:24]<=wdata[31:24];
    rdata<=mem[addr[17:2]];
  end
  endmodule
