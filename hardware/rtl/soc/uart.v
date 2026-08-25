// Omni-RISC APU — SoC: UART (memory-mapped, full duplex)
//
// Registers (base 0x40000000):
//   +0x00 TX      write a byte to transmit (8N1 @115200)
//   +0x04 STATUS  bit0 = TX busy (1 = a byte is being shifted out)
//                 bit1 = RX ready (1 = RX FIFO non-empty)
//                 bit2 = RX overrun (sticky; cleared by any +0x08 read)
//   +0x08 RXDATA  read: peek head of the RX FIFO (pops one byte if non-empty;
//                 also clears overrun). Reading when empty returns stale data
//                 without popping — software gates on STATUS[1].
//
// RX framing: 8N1 @115200, uart_rx synchronized through 2FF, start-bit
// validated by a mid-bit resample (short glitches return to IDLE), data
// LSB-first sampled at each bit center, byte pushed to a 16-deep FIFO on
// stop-bit completion. Stop-bit value is not enforced (framing errors are
// not reported); bytes are delivered regardless.
//
// Each pbus slave returns 0 on pbus_rdata when not selected, so soc_top can
// OR the slave read busses together.
module uart (
    input         clk,
    input         reset,

    // ---- pbus slave port ----
    input  [31:0] pbus_addr,
    input  [31:0] pbus_wdata,
    input  [3:0]  pbus_wen,
    input         pbus_read,
    output [31:0] pbus_rdata,

    input         uart_rx,
    output reg    uart_tx
);
    localparam CLKS_PER_BIT = 434;      // 50 MHz / 115200 baud
    // exact 4KB page decode: 0x40000000. The old [31:20] + ~addr[12] form
    // aliased the GPU page (0x40002000, bit 13) — gpu_launch's WARP_PC0
    // write (+0x00) matched the TX offset and reloaded the shift register
    // mid-frame (shell transcript carried a NUL; found under tb_soc_shell).
    wire sel = (pbus_addr[31:12] == 20'h40000);

    // ====================================================================
    // TX path (unchanged)
    // ====================================================================
    reg tx_busy;
    reg [7:0] tx_shift;
    reg [3:0] bit_cnt;                  // 0=start, 1..8=data, 9=stop
    reg [9:0] clk_cnt;                  // bit-period counter

    wire write_tx = sel && (pbus_wen != 0) && (pbus_addr[11:0] == 12'h000);

    always @(posedge clk) begin
        if (reset) begin
            tx_busy  <= 1'b0;
            tx_shift <= 8'b0;
            bit_cnt  <= 4'b0;
            clk_cnt  <= 10'b0;
        end else if (write_tx) begin
            tx_shift <= pbus_wdata[7:0];
            bit_cnt  <= 4'b0;
            clk_cnt  <= 10'b0;
            tx_busy  <= 1'b1;
        end else if (tx_busy) begin
            if (clk_cnt == CLKS_PER_BIT - 1) begin
                clk_cnt <= 10'b0;
                if (bit_cnt == 4'd9) tx_busy <= 1'b0;   // stop bit done
                else                bit_cnt <= bit_cnt + 1'b1;
            end else begin
                clk_cnt <= clk_cnt + 1'b1;
            end
        end
    end

    // 8N1: start=0, data LSB-first, stop=1
    always @(*) begin
        if (!tx_busy) uart_tx = 1'b1;
        else case (bit_cnt)
            4'd0:                    uart_tx = 1'b0;
            4'd1,4'd2,4'd3,4'd4,
            4'd5,4'd6,4'd7,4'd8:     uart_tx = tx_shift[bit_cnt-1];
            default:                 uart_tx = 1'b1;
        endcase
    end

    // ====================================================================
    // RX path: 2FF sync -> start detect w/ mid-bit validate -> data x8 ->
    // stop -> 16-byte FIFO. Separate counters from TX on purpose.
    // ====================================================================
    reg rx_ff1, rx_ff2;
    always @(posedge clk) begin
        rx_ff1 <= uart_rx;
        rx_ff2 <= rx_ff1;
    end
    wire rx_in = rx_ff2;

    localparam RX_IDLE = 2'd0, RX_START = 2'd1, RX_DATA = 2'd2, RX_STOP = 2'd3;
    reg [1:0] rx_state;
    reg [3:0] rx_bit_cnt;               // 0..7 data bits
    reg [9:0] rx_clk_cnt;
    reg [7:0] rx_shift;

    // 16-deep RX FIFO (distributed RAM: NO reset on the array, pointers and
    // count only). Pointers are 4-bit BUT occupancy uses an explicit 5-bit
    // count — 4-bit full/empty comparison cannot distinguish full from empty
    // at depth 16 (the classic wrap bug; found by tb_uart_rx case 4).
    reg [7:0] rx_fifo [0:15];
    reg [3:0] rd_ptr, wr_ptr;
    reg [4:0] rx_cnt;
    reg       rx_overrun;

    wire rx_ready = (rx_cnt != 5'd0);
    wire rx_full  = (rx_cnt == 5'd16);

    wire rx_strobe = sel && pbus_read && (pbus_addr[11:0] == 12'h008);
    wire rx_pop    = rx_strobe && rx_ready;         // pbus loads are 1 cycle

    // a completed frame pushes on its stop-bit cycle
    wire rx_push = (rx_state == RX_STOP) && (rx_clk_cnt == CLKS_PER_BIT - 1);
    // a push into a full FIFO drops the byte and sets the sticky flag; a
    // +0x08 read clears it. If both land the same cycle, the drop wins —
    // the flag must reflect that a byte was just lost.
    wire rx_overrun_next = (rx_push && rx_full) ? 1'b1
                         : (rx_strobe)          ? 1'b0
                         : rx_overrun;

    always @(posedge clk) begin
        if (reset) begin
            rx_state   <= RX_IDLE;
            rx_bit_cnt <= 4'b0;
            rx_clk_cnt <= 10'b0;
            rx_shift   <= 8'b0;
            rd_ptr     <= 4'b0;
            wr_ptr     <= 4'b0;
            rx_cnt     <= 5'b0;
            rx_overrun <= 1'b0;
        end else begin
            // ---- FIFO control ----
            if (rx_pop) rd_ptr <= rd_ptr + 1'b1;
            if (rx_push && !rx_full) begin
                rx_fifo[wr_ptr] <= rx_shift;
                wr_ptr          <= wr_ptr + 1'b1;
            end
            rx_overrun <= rx_overrun_next;

            // occupancy: push/pop may coincide (net zero when not full)
            if (rx_push && !rx_full) begin
                if (!rx_pop) rx_cnt <= rx_cnt + 1'b1;
            end else if (rx_pop) begin
                rx_cnt <= rx_cnt - 1'b1;
            end

            // ---- deserializer FSM ----
            case (rx_state)
                RX_IDLE: if (!rx_in) begin
                    rx_state   <= RX_START;
                    rx_clk_cnt <= 10'b0;
                end
                RX_START: if (rx_clk_cnt == CLKS_PER_BIT/2 - 1) begin
                    rx_clk_cnt <= 10'b0;
                    if (!rx_in) begin                       // valid start bit
                        rx_state   <= RX_DATA;
                        rx_bit_cnt <= 4'b0;
                    end else begin
                        rx_state <= RX_IDLE;                // glitch
                    end
                end else begin
                    rx_clk_cnt <= rx_clk_cnt + 1'b1;
                end
                RX_DATA: if (rx_clk_cnt == CLKS_PER_BIT - 1) begin
                    rx_clk_cnt <= 10'b0;
                    rx_shift   <= {rx_in, rx_shift[7:1]};   // LSB first
                    if (rx_bit_cnt == 4'd7) rx_state <= RX_STOP;
                    else                    rx_bit_cnt <= rx_bit_cnt + 1'b1;
                end else begin
                    rx_clk_cnt <= rx_clk_cnt + 1'b1;
                end
                RX_STOP: if (rx_clk_cnt == CLKS_PER_BIT - 1) begin
                    rx_state <= RX_IDLE;
                end else begin
                    rx_clk_cnt <= rx_clk_cnt + 1'b1;
                end
                default: rx_state <= RX_IDLE;
            endcase
        end
    end

    // read path: STATUS[0] = TX busy, STATUS[1] = RX ready, STATUS[2] = RX
    // overrun; +0x08 peeks the FIFO head
    reg [31:0] rdata;
    always @(*) begin
        rdata = 32'b0;
        if (sel && (pbus_addr[11:0] == 12'h004))
            rdata = {29'b0, rx_overrun, rx_ready, tx_busy};
        else if (sel && (pbus_addr[11:0] == 12'h008))
            rdata = {24'b0, rx_fifo[rd_ptr]};
    end
    assign pbus_rdata = rdata;
endmodule
