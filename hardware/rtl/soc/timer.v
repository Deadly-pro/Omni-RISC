// Omni-RISC APU — SoC: machine timer (CLINT)
//
// Registers (base 0x02000000):
//   +0x0000 msip             (bit0 = machine software interrupt pending)
//   +0x4000 mtimecmp[31:0]   +0x4004 mtimecmp[63:32]
//   +0xBFF8 mtime[31:0]      +0xBFFC mtime[63:32]
//
// mtime is a free-running 64-bit counter; mtip (machine timer interrupt
// pending) asserts when mtime >= mtimecmp. The handler clears it by writing
// mtimecmp forward. Each pbus slave returns 0 on pbus_rdata when not selected.
module timer (
    input         clk,
    input         reset,

    // ---- pbus slave port ----
    input  [31:0] pbus_addr,
    input  [31:0] pbus_wdata,
    input  [3:0]  pbus_wen,
    input         pbus_read,
    output [31:0] pbus_rdata,

    output       mtip,
    output       msip
);
    wire sel = (pbus_addr[31:24] == 8'h02);          // 0x02000000 region

    reg [63:0] mtime, mtimecmp;
    reg        msip_r;

    always @(posedge clk) begin
        if (reset) begin
            mtime    <= 64'b0;
            mtimecmp <= 64'b0;
            msip_r   <= 1'b0;
        end else begin
            mtime <= mtime + 1'b1;                    // free-running
            if (sel && (pbus_wen != 0)) begin
                case (pbus_addr[15:2])
                    14'h0000: msip_r           <= pbus_wdata[0];  // +0x0000
                    14'h1000: mtimecmp[31:0]  <= pbus_wdata;   // +0x4000
                    14'h1001: mtimecmp[63:32] <= pbus_wdata;   // +0x4004
                    14'h2FFE: mtime[31:0]     <= pbus_wdata;   // +0xBFF8
                    14'h2FFF: mtime[63:32]    <= pbus_wdata;   // +0xBFFC
                    default: ;                                // reserved offsets
                endcase
            end
        end
    end

    assign mtip = (mtime >= mtimecmp);
    assign msip = msip_r;

    // read path
    reg [31:0] rdata;
    always @(*) begin
        rdata = 32'b0;
        if (sel) begin
            case (pbus_addr[15:2])
                14'h0000: rdata = {31'b0, msip_r};
                14'h1000: rdata = mtimecmp[31:0];
                14'h1001: rdata = mtimecmp[63:32];
                14'h2FFE: rdata = mtime[31:0];
                14'h2FFF: rdata = mtime[63:32];
                default: rdata = 32'b0;
            endcase
        end
    end
    assign pbus_rdata = rdata;
endmodule
