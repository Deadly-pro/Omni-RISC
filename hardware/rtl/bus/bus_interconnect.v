// Omni-RISC Simple Bus Interconnect (Arbiter + Decoder)
// This module routes requests from CPU(s) to Memory and Peripherals.
`timescale 1ns / 1ps

module bus_interconnect (
    input  wire        clk,
    input  wire        rst,

    // Master Interface: CPU Core 0 ibus
    input  wire [31:0] c0_ibus_addr,
    input  wire        c0_ibus_req,
    output reg  [31:0] c0_ibus_data,
    output reg         c0_ibus_ack,

    // Master Interface: CPU Core 0 dbus
    input  wire [31:0] c0_dbus_addr,
    input  wire [31:0] c0_dbus_wdata,
    input  wire        c0_dbus_rd_en,
    input  wire        c0_dbus_wr_en,
    output reg  [31:0] c0_dbus_rdata,
    output reg         c0_dbus_ack,

    // Slave Interface: RAM (0x8000_0000)
    output reg  [31:0] ram_addr,
    output reg  [31:0] ram_wdata,
    output reg         ram_rd_en,
    output reg         ram_wr_en,
    input  wire [31:0] ram_rdata,
    input  wire        ram_ack,

    // Slave Interface: UART (0x1000_0000)
    output reg  [31:0] uart_addr,
    output reg  [31:0] uart_wdata,
    output reg         uart_rd_en,
    output reg         uart_wr_en,
    input  wire [31:0] uart_rdata,
    input  wire        uart_ack
);

    // Simple Priority Arbitration: D-Bus > I-Bus
    wire dbus_active = c0_dbus_rd_en || c0_dbus_wr_en;
    wire ibus_active = c0_ibus_req && !dbus_active;

    // Address Decoding Logic
    always @(*) begin
        // Default Slave state
        ram_addr = 32'b0; ram_wdata = 32'b0; ram_rd_en = 0; ram_wr_en = 0;
        uart_addr = 32'b0; uart_wdata = 32'b0; uart_rd_en = 0; uart_wr_en = 0;
        
        // Default Master response
        c0_dbus_rdata = 32'b0; c0_dbus_ack = 0;
        c0_ibus_data = 32'b0;  c0_ibus_ack = 0;

        // --- D-Bus Routing ---
        if (c0_dbus_addr[31:28] == 4'h8) begin // RAM: 0x8xxxxxxx
            ram_addr    = c0_dbus_addr;
            ram_wdata   = c0_dbus_wdata;
            ram_rd_en   = c0_dbus_rd_en;
            ram_wr_en   = c0_dbus_wr_en;
            c0_dbus_rdata = ram_rdata;
            c0_dbus_ack   = ram_ack;
        end else if (c0_dbus_addr[31:28] == 4'h1) begin // UART: 0x1xxxxxxx
            uart_addr   = c0_dbus_addr;
            uart_wdata  = c0_dbus_wdata;
            uart_rd_en  = c0_dbus_rd_en;
            uart_wr_en  = c0_dbus_wr_en;
            c0_dbus_rdata = uart_rdata;
            c0_dbus_ack   = uart_ack;
        end

        // --- I-Bus Routing ---
        // Priority to D-Bus ONLY if they target the same slave
        if (c0_ibus_req) begin
            if (c0_ibus_addr[31:28] == 4'h8) begin // RAM: 0x8xxxxxxx
                // If D-Bus is NOT using RAM, I-Bus can use it
                if (c0_dbus_addr[31:28] != 4'h8 || !dbus_active) begin
                    ram_addr    = c0_ibus_addr;
                    ram_rd_en   = 1'b1;
                    c0_ibus_data = ram_rdata;
                    c0_ibus_ack  = ram_ack;
                end
            end
        end
    end

endmodule
