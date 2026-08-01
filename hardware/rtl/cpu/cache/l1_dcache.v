// Omni-RISC APU — L1 data cache
//
// 4KB, 2-way set-associative, write-through, no-allocate-on-write.
// 32-byte lines (8 words), 64 sets.
//   index = addr[10:5], tag = addr[31:11], word = addr[4:2]
//
// Interface (matches tb_cache.cpp):
//   CPU:   addr, wdata, byte_en[3:0], read_en, write_en, rdata, hit, miss, ready
//   Memory: mem_addr, mem_rdata, mem_read_req, mem_read_ack (1 word per ack)
//
// Timing: a request is latched on read_en|write_en; the next cycle a hit or
// write completes (ready pulses, rdata valid). A read miss refills the line one
// word per memory transaction, then completes.
//
// NOTE: the memory interface has no write path, so write-through updates the
// cache but cannot reach the backing memory (a tb_cache contract gap).
// req_addr[1:0] is deliberately unused — the cache is word-granular (byte
// lanes ride byte_en, not the address bits)
// verilator lint_off UNUSEDSIGNAL
module l1_dcache (
    input         clk,
    input         reset,

    // ---- CPU interface ----
    input  [31:0] addr,
    input  [31:0] wdata,
    input  [3:0]  byte_en,
    input         read_en,
    input         write_en,
    output reg [31:0] rdata,
    output        hit,
    output        miss,
    output reg    ready,

    // ---- memory interface (1 word per request/ack) ----
    output reg [31:0] mem_addr,
    input  [31:0] mem_rdata,
    output reg    mem_read_req,
    input         mem_read_ack
);
    localparam TAG_W   = 21;
    localparam WORDS   = 8;
    localparam SETS    = 64;

    // MMIO bypass: addresses >= 0x10000000 are never cached
    wire mmio = (addr[31:28] != 4'b0);

    reg [TAG_W-1:0] tag  [0:1][0:SETS-1];
    reg             valid[0:1][0:SETS-1];
    reg [31:0]      data [0:1][0:SETS-1][0:WORDS-1];

    assign hit  = ((valid[0][addr[10:5]] && tag[0][addr[10:5]] == addr[31:11]) ||
                   (valid[1][addr[10:5]] && tag[1][addr[10:5]] == addr[31:11])) && ~mmio;
    assign miss = (read_en | write_en) & ~hit & ~mmio;

    localparam IDLE = 2'b00, CHECK = 2'b01, WAITM = 2'b10;
    reg [1:0] state;

    reg       req;                    // a request is latched
    reg       req_write;
    reg [31:0] req_addr, req_wdata;
    reg [3:0]  req_ben;

    reg [2:0]  fill_cnt;
    reg        fill_way;
    reg        last_way;              // way filled most recently (round-robin)
    reg        pending_write;         // a write-allocate refill is in progress
    integer    k;                     // loop index for byte-merge writes
    reg [31:0] wnew;

    // combinational hit lookup for the latched request
    wire req_hit =
        ((valid[0][req_addr[10:5]] && tag[0][req_addr[10:5]] == req_addr[31:11]) ||
         (valid[1][req_addr[10:5]] && tag[1][req_addr[10:5]] == req_addr[31:11])) &&
        (req_addr[31:28] == 4'b0);
    wire req_hit0 = valid[0][req_addr[10:5]] && tag[0][req_addr[10:5]] == req_addr[31:11];

    // pick a way for a refill: prefer an invalid way, else the least-recently filled
    function automatic pick_way(input [5:0] idx, input last);
        if (!valid[0][idx])      pick_way = 1'b0;
        else if (!valid[1][idx]) pick_way = 1'b1;
        else                     pick_way = last ? 1'b0 : 1'b1;
    endfunction

    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE; req <= 1'b0; ready <= 1'b0; mem_read_req <= 1'b0;
            rdata <= 32'b0; last_way <= 1'b0; pending_write <= 1'b0;
            for (k = 0; k < SETS; k = k + 1) begin
                valid[0][k] <= 1'b0;
                valid[1][k] <= 1'b0;
            end
        end else begin
            mem_read_req <= 1'b0;     // pulse
            ready        <= 1'b0;     // pulse
            if (read_en | write_en) begin
                req <= 1'b1; req_write <= write_en;
                req_addr <= addr; req_wdata <= wdata; req_ben <= byte_en;
            end
            case (state)
                IDLE: begin
                    if (req) state <= CHECK;
                end
                CHECK: begin
                    if (read_en | write_en) begin
                        // a new request arrived while this one was being
                        // latched — the req_* regs update at this posedge, so
                        // re-enter CHECK next cycle with the new request
                        state <= CHECK;
                    end
                    else if (req_write) begin
                        // write-through: update the cache line if present
                        if (req_hit) begin
                            // verilator lint_off BLKSEQ
                            wnew = data[req_hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]];
                            if (req_ben[0]) wnew[7:0]   = req_wdata[7:0];
                            if (req_ben[1]) wnew[15:8]  = req_wdata[15:8];
                            if (req_ben[2]) wnew[23:16] = req_wdata[23:16];
                            if (req_ben[3]) wnew[31:24] = req_wdata[31:24];
                            data[req_hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]] <= wnew;
                            // verilator lint_on BLKSEQ
                            ready <= 1'b1; req <= 1'b0; state <= IDLE;
                        end else begin
                            // write miss → write-allocate: refill, then apply
                            pending_write <= 1'b1;
                            fill_cnt <= 3'b0;
                            fill_way <= pick_way(req_addr[10:5], last_way);
                            mem_addr <= {req_addr[31:5], 5'b0};
                            mem_read_req <= 1'b1;
                            state <= WAITM;
                        end
                    end
                    else if (req_hit) begin
                        rdata <= data[req_hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]];
                        ready <= 1'b1; req <= 1'b0; state <= IDLE;
                    end
                    else begin
                        // read miss → refill the line (8 words)
                        fill_cnt <= 3'b0;
                        fill_way <= pick_way(req_addr[10:5], last_way);
                        mem_addr <= {req_addr[31:5], 5'b0};
                        mem_read_req <= 1'b1;
                        state <= WAITM;
                    end
                end
                WAITM: begin
                    if (mem_read_ack) begin
                        data[fill_way][req_addr[10:5]][fill_cnt] <= mem_rdata;
                        if (fill_cnt == 3'd7) begin
                            tag[fill_way][req_addr[10:5]]   <= req_addr[31:11];
                            valid[fill_way][req_addr[10:5]] <= 1'b1;
                            last_way <= fill_way;
                            if (pending_write) begin
                                // write-allocate: now apply the pending write
                                pending_write <= 1'b0;
                                // verilator lint_off BLKSEQ
                                wnew = data[fill_way][req_addr[10:5]][req_addr[4:2]];
                                if (req_ben[0]) wnew[7:0]   = req_wdata[7:0];
                                if (req_ben[1]) wnew[15:8]  = req_wdata[15:8];
                                if (req_ben[2]) wnew[23:16] = req_wdata[23:16];
                                if (req_ben[3]) wnew[31:24] = req_wdata[31:24];
                                data[fill_way][req_addr[10:5]][req_addr[4:2]] <= wnew;
                                // verilator lint_on BLKSEQ
                            end else begin
                                rdata <= data[fill_way][req_addr[10:5]][req_addr[4:2]];
                            end
                            ready <= 1'b1; req <= 1'b0; state <= IDLE;
                        end else begin
                            fill_cnt <= fill_cnt + 1'b1;
                            mem_addr <= {req_addr[31:5], fill_cnt + 1'b1, 2'b0};
                            mem_read_req <= 1'b1;
                        end
                    end
                end
                default: state <= IDLE;
            endcase
        end
    end
endmodule
// verilator lint_on UNUSEDSIGNAL
