// Omni-RISC APU — SoC top-level integration
//
// Wires the CPU's peripheral bus to the memory-mapped slaves and routes the
// timer interrupt into the CPU. Instruction/data memory live inside cpu_top
// (instr_bram in fetch_stage, data_bram in mem_stage); the firmware image is
// loaded into instr_bram via $readmemh("program.hex") at time zero.
//
// Memory map (see docs/memory_map.md):
//   0x00000000-0x0000FFFF  IMEM  (instr_bram, inside cpu_top)
//   0x00010000-0x0003FFFF  DMEM  (data_bram, inside cpu_top)
//   0x02000000-0x0200FFFF  CLINT / timer (mtimecmp @0x4000, mtime @0xBFF8)
//   0x40000000-0x40000FFF  UART  (TX @+0, status @+4)
//   0x40001000-0x40001FFF  GPIO  (8-bit output @+0)
module soc_top (
    input         clk,
    input         reset,
    output        uart_tx,
    input         uart_rx,       // unused (no RX path yet)
    output [7:0]  gpio_out
);
    wire [31:0] pbus_addr, pbus_wdata, pbus_rdata;
    wire [3:0]  pbus_wen;
    wire        pbus_read;
    wire        mtip;

    wire [31:0] uart_rdata, timer_rdata, gpio_rdata;

    // Phase C: the L1 caches are module-verified (tb_cache/tb_icache) but the
    // SoC pipeline integration still has a fetch-pairing bug on concurrent
    // cache refills (documented in docs/roadmap.md), so the SoC runs the
    // cacheless CPU until that is resolved.
    cpu_top #(.DMEM_FILE("program.hex"), .USE_CACHES(0)) u_cpu(
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(pbus_rdata),
        .mtip(mtip)
    );

    uart u_uart(
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(uart_rdata),
        .uart_tx(uart_tx)
    );

    timer u_timer(
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(timer_rdata),
        .mtip(mtip)
    );

    gpio u_gpio(
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(gpio_rdata),
        .gpio_out(gpio_out)
    );

    // each slave drives 0 when not selected, so ORing selects the winner
    assign pbus_rdata = uart_rdata | timer_rdata | gpio_rdata;
endmodule
