// Omni-RISC APU — Dual-core top with shared snooping bus
//
// Two cpu_top instances share a 32-bit address/data bus with MSI snooping.
// Each core has private L1 I$ + D$. Coherence is at the D$ line granularity.
//
// Interface (compatible with soc_top but 2-core):
//   clk, reset
//   Peripheral bus (shared, from cores 0/1): pbus_addr, pbus_wdata, pbus_wen, pbus_read, pbus_rdata
//   Machine timer interrupt in (from CLINT): mtip (broadcast to both cores)
//   UART: uart_tx
//   GPIO: gpio_out[7:0]
module cpu2_top #(
    parameter DMEM_FILE_0 = "",
    parameter DMEM_FILE_1 = "",
    parameter USE_CACHES = 1
) (
    input         clk,
    input         reset,

    // ---- shared peripheral bus (AXI4-Lite-like, simple decode) ----
    output [31:0] pbus_addr,
    output [31:0] pbus_wdata,
    output [3:0]  pbus_wen,
    output        pbus_read,
    output [31:0] pbus_rdata,

    // ---- machine timer interrupt (broadcast) ----
    input         mtip,

    // ---- UART ----
    output        uart_tx,

    // ---- GPIO ----
    output [7:0]  gpio_out
);

    // ---- Core 0 bus ----
    wire [31:0] pbus_addr_0, pbus_wdata_0, pbus_rdata_0;
    wire [3:0]  pbus_wen_0;
    wire        pbus_read_0;

    // Snoop interface from Core 0's perspective (watching Core 1's bus)
    wire [31:0] snoop_addr_1;
    wire        snoop_read_1, snoop_write_1, snoop_ack_1;

    // ---- Core 1 bus ----
    wire [31:0] pbus_addr_1, pbus_wdata_1, pbus_rdata_1;
    wire [3:0]  pbus_wen_1;
    wire        pbus_read_1;

    // Snoop interface from Core 1's perspective (watching Core 0's bus)
    wire [31:0] snoop_addr_0;
    wire        snoop_read_0, snoop_write_0, snoop_ack_0;

    // ---- Shared bus arbitration (simple fixed priority: core0 > core1) ----
    reg [31:0] pbus_addr_r, pbus_wdata_r;
    reg [3:0]  pbus_wen_r;
    reg        pbus_read_r;
    reg        bus_grant;  // 0 = core0, 1 = core1

    always @(*) begin
        if (pbus_read_0 | (|pbus_wen_0)) begin
            bus_grant = 1'b0;
        end else if (pbus_read_1 | (|pbus_wen_1)) begin
            bus_grant = 1'b1;
        end else begin
            bus_grant = 1'b0;
        end

        pbus_addr_r  = bus_grant ? pbus_addr_1  : pbus_addr_0;
        pbus_wdata_r = bus_grant ? pbus_wdata_1 : pbus_wdata_0;
        pbus_wen_r   = bus_grant ? pbus_wen_1   : pbus_wen_0;
        pbus_read_r  = bus_grant ? pbus_read_1  : pbus_read_0;
    end

    assign pbus_addr  = pbus_addr_r;
    assign pbus_wdata = pbus_wdata_r;
    assign pbus_wen   = pbus_wen_r;
    assign pbus_read  = pbus_read_r;

    // Route read data back to the granted core
    assign pbus_rdata_0 = (!bus_grant) ? pbus_rdata : 32'b0;
    assign pbus_rdata_1 = (bus_grant)  ? pbus_rdata : 32'b0;

    // Snoop connections: each core snoops the other's bus transactions
    assign snoop_addr_0  = pbus_addr_0;
    assign snoop_read_0  = pbus_read_0;
    assign snoop_write_0 = (|pbus_wen_0);

    assign snoop_addr_1  = pbus_addr_1;
    assign snoop_read_1  = pbus_read_1;
    assign snoop_write_1 = (|pbus_wen_1);

    // ---- Core 0 ----
    cpu_top #(.DMEM_FILE(DMEM_FILE_0), .USE_CACHES(USE_CACHES)) u_core0 (
        .clk       (clk),
        .reset     (reset),
        .pbus_addr (pbus_addr_0),
        .pbus_wdata(pbus_wdata_0),
        .pbus_wen  (pbus_wen_0),
        .pbus_read (pbus_read_0),
        .pbus_rdata(pbus_rdata_0),
        .mtip      (mtip),
        .msip      (msip_unused),
        .snoop_addr(snoop_addr_1),  // snoop core1's bus
        .snoop_read(snoop_read_1),
        .snoop_write(snoop_write_1),
        .snoop_ack (snoop_ack_1)
    );

    // ---- Core 1 ----
    cpu_top #(.DMEM_FILE(DMEM_FILE_1), .USE_CACHES(USE_CACHES)) u_core1 (
        .clk       (clk),
        .reset     (reset),
        .pbus_addr (pbus_addr_1),
        .pbus_wdata(pbus_wdata_1),
        .pbus_wen  (pbus_wen_1),
        .pbus_read (pbus_read_1),
        .pbus_rdata(pbus_rdata_1),
        .mtip      (mtip),
        .msip      (msip_unused),
        .snoop_addr(snoop_addr_0),  // snoop core0's bus
        .snoop_read(snoop_read_0),
        .snoop_write(snoop_write_0),
        .snoop_ack (snoop_ack_0)
    );

    // ---- SoC peripherals (UART, timer, GPIO) ----
    wire [31:0] uart_rdata, timer_rdata, gpio_rdata;
    wire        timer_mtip;
    wire        msip_unused;

    uart u_uart(
        .clk(clk), .reset(reset),
        .pbus_addr(pbus_addr), .pbus_wdata(pbus_wdata), .pbus_wen(pbus_wen),
        .pbus_read(pbus_read), .pbus_rdata(uart_rdata),
        .uart_tx(uart_tx)
    );

    timer u_timer(
        .clk(clk), .reset(reset),
        .pbus_addr(pbus_addr), .pbus_wdata(pbus_wdata), .pbus_wen(pbus_wen),
        .pbus_read(pbus_read), .pbus_rdata(timer_rdata),
        .mtip(timer_mtip),
        .msip(msip_unused)
    );

    gpio u_gpio(
        .clk(clk), .reset(reset),
        .pbus_addr(pbus_addr), .pbus_wdata(pbus_wdata), .pbus_wen(pbus_wen),
        .pbus_read(pbus_read), .pbus_rdata(gpio_rdata),
        .gpio_out(gpio_out)
    );

    // Mux peripheral read data
    assign pbus_rdata = uart_rdata | timer_rdata | gpio_rdata;

    // Broadcast timer interrupt
    // (CLINT drives mtip to both cores; here we just use the timer's mtip)

endmodule