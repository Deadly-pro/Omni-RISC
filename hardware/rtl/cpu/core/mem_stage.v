module mem_stage #(
    parameter DMEM_FILE = "",          // SoC sets the firmware image; bare-CPU tests leave empty
    parameter USE_CACHES = 0           // 0 = direct data_bram (pre-Phase-C), 1 = L1 D-cache
) (
      input         clk,
      input         reset,
      input         stall,

      // ---- EX/MEM bundle in (matches exec_stage outputs 1:1) ----
      input  [31:0] ex_mem_alu_result,
      input  [31:0] ex_mem_store_data,   // unused today — consumed here when LSU lands
      input  [4:0]  ex_mem_rd,
      input  [2:0]  ex_mem_funct3,       // unused today — load/store size, later
      input  [31:0] ex_mem_pc_plus4,
      input         ex_mem_jump,
      input         ex_mem_reg_write,
      input         ex_mem_mem_read,
      input         ex_mem_mem_write,    // unused today
      input         ex_mem_is_lr,        // LR.W instruction
      input         ex_mem_is_sc,        // SC.W instruction

      // ---- MEM/WB bundle out ----
      output reg [31:0] mem_wb_alu_result,
      output reg [31:0] mem_wb_rdata,
      output reg [4:0]  mem_wb_rd,
      output reg [31:0] mem_wb_pc_plus4,
      output reg        mem_wb_jump,
      output reg        mem_wb_mem_read,    // WB's mux select for load_data
      output reg        mem_wb_reg_write,
      output reg [1:0] mem_wb_ld_lsb,
      output reg [2:0] mem_wb_ld_funct3,

      // ---- peripheral bus (SoC slaves: UART/TIMER/GPIO) ----
      output [31:0] pbus_addr,      // raw address (ex_mem_alu_result)
      output [31:0] pbus_wdata,     // lane-aligned write data (from LSU)
      output [3:0]  pbus_wen,       // per-byte write enables (from LSU)
      output        pbus_read,      // ex_mem_mem_read
      input  [31:0] pbus_rdata,     // muxed read data from the SoC decoder

      // ---- L1 D-cache freeze ----
      output        mem_stall,       // 1 = a cache op is pending → freeze the pipeline

      // ---- snoop interface (for dual-core MSI coherence) ----
      input  [31:0] snoop_addr,
      input         snoop_read,
      input         snoop_write,
      output        snoop_ack
  );
  wire [31:0] dmem_addr, dmem_wdata_cpu, dmem_rdata;
wire [31:0] dmem_wdata_mem;   // write data from cache to data_bram
  wire [3:0]  dmem_wen;

  // data_bram decodes addr[17:2] (256KB, aliased every 256KB). The pbus slaves
  // live at 0x02000000+ (timer) and 0x40000000+ (UART/GPIO), so BOTH the low
  // 256KB AND the 0x100000-0x13FFFF aliased window are data — the riscv-arch-test
  // compliance images link at 0x100000 and store their signature/tohost there.
  wire data_cs = (ex_mem_alu_result < 32'h0004_0000) ||
                 ((ex_mem_alu_result >= 32'h0010_0000) && (ex_mem_alu_result < 32'h0014_0000));
  wire is_cache_op = data_cs & (ex_mem_mem_read | ex_mem_mem_write);

  // data_bram glue (addr/wen differ per cache config; wdata is always the LSU's)
  wire [31:0] dbram_addr;
  wire [3:0]  dbram_wen;
  wire [31:0] load_rdata;    // value mem_wb_rdata captures
  wire        mem_hold;      // 1 = hold MEM/WB + freeze the pipeline

  lsu u_lsu(
      .addr(ex_mem_alu_result), .store_data(ex_mem_store_data),
      .funct3(ex_mem_funct3), .mem_write(ex_mem_mem_write),
      .dmem_addr(dmem_addr), .dmem_wdata(dmem_wdata_cpu), .dmem_wen(dmem_wen)
  );
  data_bram #(.MemFile(DMEM_FILE)) u_dbram(
      .clk(clk), .addr(dbram_addr), .wdata(dmem_wdata_cpu),
      .wen(dbram_wen), .rdata(dmem_rdata)
  );

  generate
    if (USE_CACHES) begin : g_dcache
      // ---- L1 D-cache with MSI snooping: refills from data_bram (0-latency comb read, so ack =
      // request), write-through reaches data_bram via a ONE-SHOT direct store
      // (the cache's own memory interface has no write path) ----
      reg issue_done;             // the cache op has been presented to the cache
      wire dc_hit, dc_miss, dc_ready;
      wire [31:0] dc_rdata;
      wire [31:0] dc_mem_addr;
      wire        dc_mem_req, dc_mem_ack;
      wire        dc_mem_write_req, dc_mem_write_ack;

      l1_dcache_msi u_dcache(
          .clk(clk), .reset(reset),
          .addr(dmem_addr), .wdata(dmem_wdata_cpu), .byte_en(dmem_wen),
          .read_en(is_cache_op & ~issue_done & ex_mem_mem_read),
          .write_en(is_cache_op & ~issue_done & ex_mem_mem_write),
          .is_lr(is_cache_op & ~issue_done & ex_mem_mem_read & ex_mem_is_lr),
          .is_sc(is_cache_op & ~issue_done & ex_mem_mem_write & ex_mem_is_sc),
          .rdata(dc_rdata), .hit(dc_hit), .miss(dc_miss), .ready(dc_ready),
          .mem_addr(dc_mem_addr), .mem_rdata(dmem_rdata),
          .mem_read_req(dc_mem_req), .mem_read_ack(dc_mem_ack),
          .mem_write_req(dc_mem_write_req), .mem_wdata(dmem_wdata_mem), .mem_write_ack(dc_mem_write_ack),
          .snoop_addr(snoop_addr), .snoop_read(snoop_read), .snoop_write(snoop_write),
          .snoop_ack(snoop_ack)
      );
      assign dc_mem_ack = dc_mem_req;      // data_bram read is combinational
      assign dc_mem_write_ack = dc_mem_write_req; // data_bram write is combinational

      // issue the request once, hold until the cache reports ready
      always @(posedge clk) begin
          if (reset) issue_done <= 1'b0;
          else if (is_cache_op && dc_ready) issue_done <= 1'b0;
          else if (is_cache_op && ~issue_done) issue_done <= 1'b1;
          else if (~is_cache_op) issue_done <= 1'b0;
      end

      assign mem_hold    = is_cache_op & ~dc_ready;
      assign load_rdata  = data_cs ? dc_rdata : pbus_rdata;
      // refill reads AND cache write-through (SC.W / write-allocate) take the
      // cache's address; otherwise the LSU's address
      assign dbram_addr  = (dc_mem_req | dc_mem_write_req) ? dc_mem_addr : dmem_addr;
      // cache write-through is a full-word store; LSU writes use its byte enables
      assign dbram_wen   = dc_mem_write_req ? 4'b1111
                                            : (dmem_wen & {4{data_cs & ~issue_done}});
    end else begin : g_nocache
      assign mem_hold    = 1'b0;
      assign load_rdata  = data_cs ? dmem_rdata : pbus_rdata;
      assign dbram_addr  = dmem_addr;
      assign dbram_wen   = data_cs ? dmem_wen : 4'b0;
    end
  endgenerate

  assign mem_stall  = mem_hold;
  // pbus writes only ever target peripherals (a data-window store is the
  // cache's business); gating keeps the slaves from seeing cache-op writes
  assign pbus_addr  = ex_mem_alu_result;
  assign pbus_wdata = dmem_wdata_cpu;
  assign pbus_wen   = data_cs ? 4'b0 : dmem_wen;
  assign pbus_read  = ex_mem_mem_read;

always @(posedge clk)begin
if (reset)begin
    mem_wb_alu_result <= 32'b0;
    mem_wb_rdata      <= 32'b0;
    mem_wb_rd <= 5'b0;
    mem_wb_pc_plus4 <= 32'b0;
    mem_wb_jump <= 1'b0;
    mem_wb_mem_read <= 1'b0;
    mem_wb_reg_write <= 1'b0;
    mem_wb_ld_lsb    <= 2'b00;
    mem_wb_ld_funct3 <= 3'b000;
    end
else if (!stall && !mem_hold) begin
    mem_wb_alu_result <= ex_mem_alu_result;
    mem_wb_rdata      <= load_rdata;
    mem_wb_rd <= ex_mem_rd;
    mem_wb_pc_plus4 <= ex_mem_pc_plus4;
    mem_wb_jump <= ex_mem_jump;
    mem_wb_mem_read <= ex_mem_mem_read;
    mem_wb_reg_write <= ex_mem_reg_write;
    mem_wb_ld_lsb    <= ex_mem_alu_result[1:0];
    mem_wb_ld_funct3 <= ex_mem_funct3;
end
end
endmodule
