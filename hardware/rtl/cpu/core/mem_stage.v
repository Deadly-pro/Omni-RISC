module mem_stage #(
    parameter DMEM_FILE = "",          // SoC sets the firmware image; bare-CPU tests leave empty
    parameter USE_CACHES = 0,          // 0 = direct data_bram (pre-Phase-C), 1 = L1 D-cache
    parameter SHARED_MEM = 0           // 1 = dcache backing memory is external (dual-core);
                                       //     implies USE_CACHES=1
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
      output        snoop_ack,

      // ---- shared backing memory interface (SHARED_MEM=1 only) ----
      // the dcache refills / write-throughs via these ports to a shared data_bram
      // owned by dual_core_top; when SHARED_MEM=0 they are unused
      output [31:0] mem_if_addr,
      input  [31:0] mem_if_rdata,
      output        mem_if_read_req,
      input         mem_if_read_ack,
      output        mem_if_write_req,
      output [31:0] mem_if_wdata,
      input         mem_if_write_ack
  );
  wire [31:0] dmem_addr, dmem_wdata_cpu, dmem_rdata;
  wire [31:0] dmem_wdata_mem;   // write data from cache to backing memory
  wire [3:0]  dmem_wen;

  // data_bram decodes addr[17:2] (256KB, aliased every 256KB). The pbus slaves
  // live at 0x02000000+ (timer) and 0x40000000+ (UART/GPIO), so BOTH the low
  // 256KB AND the 0x100000-0x13FFFF aliased window are data — the riscv-arch-test
  // compliance images link at 0x100000 and store their signature/tohost there.
  wire data_cs = (ex_mem_alu_result < 32'h0004_0000) ||
                 ((ex_mem_alu_result >= 32'h0010_0000) && (ex_mem_alu_result < 32'h0014_0000));
  wire is_cache_op = data_cs & (ex_mem_mem_read | ex_mem_mem_write);

  // backing memory glue (addr/wen differ per cache config; wdata is the LSU's)
  wire [31:0] dbram_addr;
  wire [3:0]  dbram_wen;
  wire [31:0] load_rdata;    // value mem_wb_rdata captures
  wire        mem_hold;      // 1 = hold MEM/WB + freeze the pipeline

  lsu u_lsu(
      .addr(ex_mem_alu_result), .store_data(ex_mem_store_data),
      .funct3(ex_mem_funct3), .mem_write(ex_mem_mem_write),
      .dmem_addr(dmem_addr), .dmem_wdata(dmem_wdata_cpu), .dmem_wen(dmem_wen)
  );

  // Backing data memory. Always instantiated (so the tb_cpu_top / compliance
  // harnesses can peek u_mem->u_dbram->mem); in SHARED_MEM mode the internal
  // bram is unused (dcache refills/write-throughs via the external mem_if_*).
  wire [31:0] bram_rdata;
  data_bram #(.MemFile(DMEM_FILE)) u_dbram(
      .clk(clk), .addr(dbram_addr), .wdata(dmem_wdata_cpu),
      .wen(dbram_wen), .rdata(bram_rdata)
  );
  assign dmem_rdata = SHARED_MEM ? mem_if_rdata : bram_rdata;

  generate
    if (USE_CACHES) begin : g_dcache
      // ---- L1 D-cache with MSI snooping ----
      reg issue_done;             // the cache op has been presented to the cache
      wire dc_hit, dc_miss, dc_ready;
      wire [31:0] dc_rdata;
      wire [31:0] dc_mem_addr;
      wire        dc_mem_req, dc_mem_ack;
      wire        dc_mem_write_req, dc_mem_write_ack;

      l1_dcache_msi u_dcache(
          .clk(clk), .reset(reset),
          .addr(dmem_addr), .wdata(dmem_wdata_cpu), .byte_en(dmem_wen),
          // SC.W has ex_mem_mem_read=1 (its result rides the load path to WB),
          // but the CACHE must see it as write-only, else CHECK's
          // `read_en | write_en` re-enter never resolves the SC.
          .read_en(is_cache_op & ~issue_done & ex_mem_mem_read & ~ex_mem_is_sc),
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

      if (SHARED_MEM) begin : g_dc_shared
        // backing memory is external: expose the dcache memory ports directly
        assign mem_if_addr      = dc_mem_addr;
        assign mem_if_read_req  = dc_mem_req;
        assign dc_mem_ack       = mem_if_read_ack;
        assign mem_if_write_req = dc_mem_write_req;
        assign mem_if_wdata     = dmem_wdata_mem;
        assign dc_mem_write_ack = mem_if_write_ack;
        assign dbram_addr       = 32'b0;
        assign dbram_wen        = 4'b0;
      end else begin : g_dc_internal
        // backing memory is the internal data_bram, whose read is now
        // REGISTERED (BRAM mapping): a refill word is valid every 2nd cycle
        // (address presented → latched → valid). Pulse the read ack on the
        // valid cycles; the dcache holds mem_read_req (level) and samples on
        // each ack, so 8-word refills take 16 cycles. Writes stay synchronous.
        reg dc_rd_tog;
        always @(posedge clk) begin
            if (reset) dc_rd_tog <= 1'b0;
            else if (dc_mem_req) dc_rd_tog <= ~dc_rd_tog;
            else                 dc_rd_tog <= 1'b0;
        end
        assign dc_mem_ack       = dc_mem_req & dc_rd_tog;
        assign dc_mem_write_ack = dc_mem_write_req;
        assign mem_if_addr      = 32'b0;
        assign mem_if_read_req  = 1'b0;
        assign mem_if_write_req = 1'b0;
        assign mem_if_wdata     = 32'b0;
        // refill reads AND cache write-through (SC.W / write-allocate) take the
        // cache's address; otherwise the LSU's address
        assign dbram_addr = (dc_mem_req | dc_mem_write_req) ? dc_mem_addr : dmem_addr;
        // cache write-through is a full-word store; LSU writes use byte enables
        assign dbram_wen  = dc_mem_write_req ? 4'b1111
                                             : (dmem_wen & {4{data_cs & ~issue_done}});
      end

      // issue the request once, hold until the cache reports ready
      always @(posedge clk) begin
          if (reset) issue_done <= 1'b0;
          else if (is_cache_op && dc_ready) issue_done <= 1'b0;
          else if (is_cache_op && ~issue_done) issue_done <= 1'b1;
          else if (~is_cache_op) issue_done <= 1'b0;
      end

      assign mem_hold    = is_cache_op & ~dc_ready;
      assign load_rdata  = data_cs ? dc_rdata : pbus_rdata;
    end else begin : g_nocache
      // data_bram read is registered → a data-window load's value is valid one
      // cycle after its address. Hold the pipeline for exactly one cycle (a
      // toggle flips while the load sits in MEM) so MEM/WB captures the load's
      // data together with the load's rd; consecutive loads each get their own
      // hold. Peripheral (pbus) loads are combinational and unaffected.
      reg dbram_hold;
      always @(posedge clk) begin
          if (reset) dbram_hold <= 1'b0;
          else if (data_cs & ex_mem_mem_read) dbram_hold <= ~dbram_hold;
          else                                dbram_hold <= 1'b0;
      end
      assign mem_hold    = (data_cs & ex_mem_mem_read) & ~dbram_hold;
      assign load_rdata  = data_cs ? dmem_rdata : pbus_rdata;
      assign dbram_addr  = dmem_addr;
      assign dbram_wen   = data_cs ? dmem_wen : 4'b0;
      assign mem_if_addr      = 32'b0;
      assign mem_if_read_req  = 1'b0;
      assign mem_if_write_req = 1'b0;
      assign mem_if_wdata     = 32'b0;
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
