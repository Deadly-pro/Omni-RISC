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
//   0x40002000-0x40002FFF  GPU   (cmd regs; kernel flashed via IMEM_FILE)
module soc_top (
    input         clk,
    input         reset,
    output        uart_tx,
    input         uart_rx,       // serial RX (115200-8N1)
    output [7:0]  gpio_out,
    output [31:0] debug_pc
);
    wire [31:0] pbus_addr, pbus_wdata, pbus_rdata;
    wire [3:0]  pbus_wen;
    wire        pbus_read;
    wire        mtip;
    wire        msip;
    wire        gpu_done;

    wire [31:0] uart_rdata, timer_rdata, gpio_rdata, gpu_rdata;

    // Phase C: the L1 caches are module-verified (tb_cache/tb_icache) but the
    // SoC pipeline integration still has a fetch-pairing bug on concurrent
    // cache refills (documented in docs/roadmap.md), so the SoC runs the
    // cacheless CPU until that is resolved.
    wire snoop_ack_unused;
    wire mem_if_read_ack_unused, mem_if_write_ack_unused;
    wire [31:0] mem_if_addr_unused, mem_if_wdata_unused;
    wire        mem_if_read_req_unused, mem_if_write_req_unused;
    cpu_top #(.DMEM_FILE("program.hex"), .USE_CACHES(0)) u_cpu(
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(pbus_rdata),
        .mtip(mtip),
        .msip(msip),
        .snoop_addr(32'b0),
        .snoop_read(1'b0),
        .snoop_write(1'b0),
        .snoop_ack(snoop_ack_unused),
        .mem_if_addr(mem_if_addr_unused),
        .mem_if_rdata(32'b0),
        .mem_if_read_req(mem_if_read_req_unused),
        .mem_if_read_ack(mem_if_read_ack_unused),
        .mem_if_write_req(mem_if_write_req_unused),
        .mem_if_wdata(mem_if_wdata_unused),
        .mem_if_write_ack(mem_if_write_ack_unused),
        .debug_pc(debug_pc)
    );

    uart u_uart(
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(uart_rdata),
        .uart_rx(uart_rx),
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
        .mtip(mtip),
        .msip(msip),
        .gpu_msip(gpu_done)
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

    // GPU: pbus slave @0x40002000. Kernel hex flashed into imem at time zero;
    // CPU dispatches via cmd regs, reads results back via the +0x14 readback.
    wire        gpu_ready_unused;
    wire [3:0]  gpu_active_unused;
    gpu_top #(.IMEM_FILE("gpu_demo.hex")) u_gpu(
        .clk(clk),
        .reset(reset),
        .pbus_addr(pbus_addr),
        .pbus_wdata(pbus_wdata),
        .pbus_wen(pbus_wen),
        .pbus_read(pbus_read),
        .pbus_rdata(gpu_rdata),
        .pbus_ready(gpu_ready_unused),
        .active_warps(gpu_active_unused),
        .gpu_done(gpu_done)
    );

    // each slave drives 0 when not selected, so ORing selects the winner
    assign pbus_rdata = uart_rdata | timer_rdata | gpio_rdata | gpu_rdata;
endmodule
