 module data_bram #(
    parameter MemFile = ""            // empty = zeroed (bare-CPU tests); SoC sets the firmware image
 ) (
      input             clk,
      input      [31:0] addr,      // word-aligned, from the LSU
      input      [31:0] wdata,
      input      [3:0]  wen,
      output reg [31:0] rdata          // registered read; valid one cycle after addr
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
  // REGISTERED read (BRAM-mappable). Consumers must tolerate 1-cycle latency:
  // mem_stage adds a 1-cycle hold for direct loads and pulses cache refill acks
  // on the cycles the data is valid (see mem_stage.v g_nocache / g_dc_internal).
  always @(posedge clk) rdata <= mem[addr[17:2]];
  endmodule
