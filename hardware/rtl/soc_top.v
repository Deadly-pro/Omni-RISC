/*
 * soc_top.v - Omni-RISC Top-Level System-on-Chip (MVP Version)
 */
`timescale 1ns / 1ps

module soc_top (
    input wire clk,
    input wire rst_n,
    
    // External interfaces
    output wire uart_tx
);

    wire rst = !rst_n;

    // --- Core 0 Instruction Bus ---
    wire [31:0] c0_ibus_addr;
    wire        c0_ibus_req;
    wire [31:0] c0_ibus_data;
    wire        c0_ibus_ack;

    // --- Core 0 Data Bus ---
    wire [31:0] c0_dbus_addr;
    wire [31:0] c0_dbus_wdata;
    wire        c0_dbus_rd_en;
    wire        c0_dbus_wr_en;
    wire [31:0] c0_dbus_rdata;
    wire        c0_dbus_ack;

    // --- Core 0 (RV32IMACV) ---
    risc_core #(
        .HART_ID(32'h0)
    ) core0 (
        .clk(clk),
        .rst(rst),
        
        .ibus_addr_out(c0_ibus_addr),
        .ibus_req_out(c0_ibus_req),
        .ibus_data_in(c0_ibus_data),
        .ibus_ack_in(c0_ibus_ack),
        
        .dbus_addr_out(c0_dbus_addr),
        .dbus_write_data_out(c0_dbus_wdata),
        .dbus_read_en_out(c0_dbus_rd_en),
        .dbus_write_en_out(c0_dbus_wr_en),
        .dbus_read_data_in(c0_dbus_rdata),
        .dbus_ack_in(c0_dbus_ack)
    );

    // --- Bus Interconnect ---
    wire [31:0] ram_addr, ram_wdata, ram_rdata;
    wire        ram_rd_en, ram_wr_en, ram_ack;

    wire [31:0] uart_addr, uart_wdata, uart_rdata;
    wire        uart_rd_en, uart_wr_en, uart_ack;

    bus_interconnect arbiter (
        .clk(clk),
        .rst(rst),
        
        .c0_ibus_addr(c0_ibus_addr),
        .c0_ibus_req(c0_ibus_req),
        .c0_ibus_data(c0_ibus_data),
        .c0_ibus_ack(c0_ibus_ack),
        
        .c0_dbus_addr(c0_dbus_addr),
        .c0_dbus_wdata(c0_dbus_wdata),
        .c0_dbus_rd_en(c0_dbus_rd_en),
        .c0_dbus_wr_en(c0_dbus_wr_en),
        .c0_dbus_rdata(c0_dbus_rdata),
        .c0_dbus_ack(c0_dbus_ack),
        
        .ram_addr(ram_addr),
        .ram_wdata(ram_wdata),
        .ram_rd_en(ram_rd_en),
        .ram_wr_en(ram_wr_en),
        .ram_rdata(ram_rdata),
        .ram_ack(ram_ack),
        
        .uart_addr(uart_addr),
        .uart_wdata(uart_wdata),
        .uart_rd_en(uart_rd_en),
        .uart_wr_en(uart_wr_en),
        .uart_rdata(uart_rdata),
        .uart_ack(uart_ack)
    );

    // --- RAM (Slave at 0x80000000) ---
    simple_ram #(
        .MEM_SIZE(16384), // 64KB for now
        .INIT_FILE("program.hex")
    ) ram0 (
        .clk(clk),
        .rst(rst),
        .addr(ram_addr),
        .wdata(ram_wdata),
        .rd_en(ram_rd_en),
        .wr_en(ram_wr_en),
        .rdata(ram_rdata),
        .ack(ram_ack)
    );

    // --- UART (Slave at 0x10000000) ---
    simple_uart uart0 (
        .clk(clk),
        .rst(rst),
        .addr(uart_addr),
        .wdata(uart_wdata),
        .rd_en(uart_rd_en),
        .wr_en(uart_wr_en),
        .rdata(uart_rdata),
        .ack(uart_ack),
        .tx(uart_tx)
    );

endmodule
