// Omni-RISC APU — Dual-Core Top with Shared PBUS and Coherent Shared Memory
//
// Two cpu_top instances with L1 caches (USE_CACHES=1) share:
//   - a peripheral bus (UART/TIMER/GPIO) via pbus_arbiter
//   - ONE physical data_bram, the coherent backing memory, via a fixed-priority
//     arbiter on each core's dcache memory interface
//
// MSI snooping: each core's L1 D-cache snoops the OTHER core's data-cache
// memory transactions (mem_if_*), so a read of an M line forces a writeback
// and a write invalidates. The pbus is NOT snooped — peripherals aren't cached.
module dual_core_top (
    input         clk,
    input         reset,
    output        uart_tx,
    input         uart_rx,
    output [7:0]  gpio_out
);

    // Core 0 PBUS
    wire [31:0] core0_addr, core0_wdata, core0_rdata;
    wire [3:0]  core0_wen;
    wire        core0_read, core0_ready;

    // Core 1 PBUS
    wire [31:0] core1_addr, core1_wdata, core1_rdata;
    wire [3:0]  core1_wen;
    wire        core1_read, core1_ready;

    // Shared PBUS to peripherals
    wire [31:0] pbus_addr, pbus_wdata, pbus_rdata;
    wire [3:0]  pbus_wen;
    wire        pbus_read, pbus_ready;

    // Arbiter grant state
    wire [1:0]  arb_grant_state;

    // Shared mtip (timer interrupt pending)
    wire        mtip;

    // Snoop connections (from the OTHER core's dcache memory transactions)
    wire [31:0] core0_snoop_addr, core1_snoop_addr;
    wire        core0_snoop_read, core1_snoop_read;
    wire        core0_snoop_write, core1_snoop_write;
    wire        core0_snoop_ack, core1_snoop_ack;

    // Shared backing memory interfaces
    wire [31:0] core0_mem_addr, core1_mem_addr;
    wire [31:0] core0_mem_rdata, core1_mem_rdata;
    wire        core0_mem_read_req, core1_mem_read_req;
    wire        core0_mem_read_ack, core1_mem_read_ack;
    wire        core0_mem_write_req, core1_mem_write_req;
    wire [31:0] core0_mem_wdata, core1_mem_wdata;
    wire        core0_mem_write_ack, core1_mem_write_ack;

    // Shared data_bram (single-ported)
    wire [31:0] bram_addr;
    wire [31:0] bram_wdata;
    wire [3:0]  bram_wen;
    wire [31:0] bram_rdata;
    wire        grant_core0;   // 0 = core0, 1 = core1 (which core owns the bram port)

    // Fixed-priority: core0 > core1
    assign grant_core0 = core0_mem_read_req | core0_mem_write_req;
    assign bram_addr   = grant_core0 ? core0_mem_addr  : core1_mem_addr;
    assign bram_wdata  = grant_core0 ? core0_mem_wdata : core1_mem_wdata;
    assign bram_wen    = grant_core0 ? {4{core0_mem_write_req}} : {4{core1_mem_write_req}};

    data_bram u_shared_dbram (
        .clk(clk), .addr(bram_addr), .wdata(bram_wdata),
        .wen(bram_wen), .rdata(bram_rdata)
    );

    // Ack and read-data routing (data_bram read is combinational)
    assign core0_mem_read_ack  = core0_mem_read_req;   // served whenever requested
    assign core0_mem_write_ack = core0_mem_write_req;
    assign core0_mem_rdata     = bram_rdata;           // core0 wins when both request
    assign core1_mem_read_ack  = core1_mem_read_req & ~grant_core0;
    assign core1_mem_write_ack = core1_mem_write_req & ~grant_core0;
    assign core1_mem_rdata     = grant_core0 ? 32'b0 : bram_rdata;

    // PBUS arbiter
    pbus_arbiter u_arbiter (
        .clk(clk),
        .reset(reset),
        .core0_addr(core0_addr),
        .core0_wdata(core0_wdata),
        .core0_wen(core0_wen),
        .core0_read(core0_read),
        .core0_rdata(core0_rdata),
        .core0_ready(core0_ready),
        .core1_addr(core1_addr),
        .core1_wdata(core1_wdata),
        .core1_wen(core1_wen),
        .core1_read(core1_read),
        .core1_rdata(core1_rdata),
        .core1_ready(core1_ready),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(pbus_rdata),
        .pbus_ready(pbus_ready),
        .grant_state(arb_grant_state)
    );

    // Core 0
    cpu_top #(
        .DMEM_FILE("program.hex"),
        .USE_CACHES(1),
        .SHARED_MEM(1)
    ) u_core0 (
        .clk(clk),
        .reset(reset),
        .pbus_addr(core0_addr),
        .pbus_wdata(core0_wdata),
        .pbus_wen(core0_wen),
        .pbus_read(core0_read),
        .pbus_rdata(core0_rdata),
        .mtip(mtip),
        .snoop_addr(core0_snoop_addr),
        .snoop_read(core0_snoop_read),
        .snoop_write(core0_snoop_write),
        .snoop_ack(core0_snoop_ack),
        .mem_if_addr(core0_mem_addr),
        .mem_if_rdata(core0_mem_rdata),
        .mem_if_read_req(core0_mem_read_req),
        .mem_if_read_ack(core0_mem_read_ack),
        .mem_if_write_req(core0_mem_write_req),
        .mem_if_wdata(core0_mem_wdata),
        .mem_if_write_ack(core0_mem_write_ack)
    );

    // Core 1
    cpu_top #(
        .DMEM_FILE("program.hex"),
        .USE_CACHES(1),
        .SHARED_MEM(1)
    ) u_core1 (
        .clk(clk),
        .reset(reset),
        .pbus_addr(core1_addr),
        .pbus_wdata(core1_wdata),
        .pbus_wen(core1_wen),
        .pbus_read(core1_read),
        .pbus_rdata(core1_rdata),
        .mtip(mtip),
        .snoop_addr(core1_snoop_addr),
        .snoop_read(core1_snoop_read),
        .snoop_write(core1_snoop_write),
        .snoop_ack(core1_snoop_ack),
        .mem_if_addr(core1_mem_addr),
        .mem_if_rdata(core1_mem_rdata),
        .mem_if_read_req(core1_mem_read_req),
        .mem_if_read_ack(core1_mem_read_ack),
        .mem_if_write_req(core1_mem_write_req),
        .mem_if_wdata(core1_mem_wdata),
        .mem_if_write_ack(core1_mem_write_ack)
    );

    // MSI snooping: each core's dcache watches the OTHER core's data-cache
    // memory transactions (refills / write-throughs) — the coherent activity.
    assign core0_snoop_addr  = core1_mem_addr;
    assign core0_snoop_read  = core1_mem_read_req;
    assign core0_snoop_write = core1_mem_write_req;

    assign core1_snoop_addr  = core0_mem_addr;
    assign core1_snoop_read  = core0_mem_read_req;
    assign core1_snoop_write = core0_mem_write_req;

    // Peripherals on shared PBUS
    wire [31:0] uart_rdata, timer_rdata, gpio_rdata;

    uart u_uart (
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(uart_rdata),
        .uart_tx(uart_tx)
    );

    timer u_timer (
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(timer_rdata),
        .mtip(mtip)
    );

    gpio u_gpio (
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(gpio_rdata),
        .gpio_out(gpio_out)
    );

    // PBUS decoder - route to correct peripheral
    reg [31:0] pbus_rdata_mux;
    reg        pbus_ready_reg;

    always @(*) begin
        pbus_rdata_mux = 32'b0;
        pbus_ready_reg = 1'b1;  // peripherals are single-cycle

        if (pbus_addr[31:24] == 8'h40 && pbus_addr[15:12] == 4'h0) begin  // UART 0x4000_0xxx
            pbus_rdata_mux = uart_rdata;
        end else if (pbus_addr[31:16] == 16'h0200) begin  // Timer 0x0200_xxxx
            pbus_rdata_mux = timer_rdata;
        end else if (pbus_addr[31:24] == 8'h40 && pbus_addr[15:12] == 4'h1) begin  // GPIO 0x4000_1xxx
            pbus_rdata_mux = gpio_rdata;
        end
    end

    assign pbus_rdata = pbus_rdata_mux;
    assign pbus_ready = pbus_ready_reg;

endmodule
