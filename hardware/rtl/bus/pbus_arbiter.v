// Omni-RISC APU — Simple PBUS Arbiter for Dual-Core
//
// Two CPU cores share a single peripheral bus. Round-robin arbitration.
// Simple request/grant handshake - no full AXI complexity needed for peripherals.
module pbus_arbiter (
    input         clk,
    input         reset,

    // Core 0 PBUS
    input  [31:0] core0_addr,
    input  [31:0] core0_wdata,
    input  [3:0]  core0_wen,
    input         core0_read,
    output [31:0] core0_rdata,
    output        core0_ready,

    // Core 1 PBUS
    input  [31:0] core1_addr,
    input  [31:0] core1_wdata,
    input  [3:0]  core1_wen,
    input         core1_read,
    output [31:0] core1_rdata,
    output        core1_ready,

    // Shared PBUS to slaves
    output [31:0] pbus_addr,
    output [31:0] pbus_wdata,
    output [3:0]  pbus_wen,
    output        pbus_read,
    input  [31:0] pbus_rdata,
    input         pbus_ready,

    // Arbitration status (for snoop)
    output [1:0]  grant_state  // 0=IDLE, 1=CORE0, 2=CORE1
);

    // Arbitration state
    reg grant_core0;
    reg [1:0] req_state;  // 0=IDLE, 1=CORE0, 2=CORE1

    // Request detection
    wire core0_req = core0_read | (|core0_wen);
    wire core1_req = core1_read | (|core1_wen);

    // Simple round-robin arbitration
    always @(posedge clk) begin
        if (reset) begin
            grant_core0 <= 1'b1;
            req_state <= 2'b00;
        end else begin
            case (req_state)
                2'b00: begin  // IDLE
                    if (core0_req && core1_req) begin
                        grant_core0 <= ~grant_core0;  // alternate
                        req_state <= grant_core0 ? 2'b01 : 2'b10;
                    end else if (core0_req) begin
                        req_state <= 2'b01;
                    end else if (core1_req) begin
                        req_state <= 2'b10;
                    end
                end
                2'b01: begin  // CORE0 granted
                    if (pbus_ready) req_state <= 2'b00;
                end
                2'b10: begin  // CORE1 granted
                    if (pbus_ready) req_state <= 2'b00;
                end
                default: req_state <= 2'b00;
            endcase
        end
    end

    // Mux selected core to shared bus
    assign pbus_addr  = (req_state == 2'b01) ? core0_addr  : core1_addr;
    assign pbus_wdata = (req_state == 2'b01) ? core0_wdata : core1_wdata;
    assign pbus_wen   = (req_state == 2'b01) ? core0_wen   : core1_wen;
    assign pbus_read  = (req_state == 2'b01) ? core0_read  : core1_read;

    // Route response back to requesting core
    assign core0_rdata = pbus_rdata;
    assign core1_rdata = pbus_rdata;
    assign core0_ready = (req_state == 2'b01) && pbus_ready;
    assign core1_ready = (req_state == 2'b10) && pbus_ready;
    assign grant_state = req_state;

endmodule
