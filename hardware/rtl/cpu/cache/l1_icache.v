// Omni-RISC APU — L1 instruction cache
//
// 4KB, direct-mapped, read-only. 32-byte lines (8 words), 128 lines.
//   index = pc[11:5], tag = pc[31:12], word = pc[4:2]
//
// Interface (matches l1_dcache's memory-side contract):
//   CPU:   pc, rdata, miss
//   Memory: mem_addr, mem_rdata, mem_read_req, mem_read_ack (1 word per ack)
//
// Timing goal: a HIT must have the same latency as the current registered
// instr_bram read (rdata updates at the posedge, one cycle after pc is
// stable), so the fetch stage stalls ONLY on misses.
//
// A miss latches the address and refills the 8-word line one word per memory
// transaction. rdata is deliberately LEFT ALONE during the refill: it holds
// the last instruction that was actually presented to IF/ID. When the line
// lands, the cache returns to IDLE and the fetch resumes at the live pc with
// a normal IDLE hit-read — so whatever IF/ID was holding before the miss
// (e.g. the last word of the previous line) is still paired with its correct
// instruction. Overwriting rdata with the live pc's line during the refill
// would pair the refilled instruction with the STALE if_id_pc and corrupt the
// fetch stream (a stranded instruction silently dropped). A redirect that
// lands mid-refill is handled by the existing flush (IF/ID → NOP) and a fresh
// IDLE hit-read at the redirected pc.
// pc[1:0] is deliberately unused — fetches are word-aligned
// verilator lint_off UNUSEDSIGNAL
module l1_icache (
    input         clk,
    input         reset,

    // ---- CPU interface ----
    input  [31:0] pc,            // fetch address (always valid, like instr_bram)
    input         fetch_en,      // 1 = the fetch stage is actually consuming (== !stall);
                                 // gates rdata so it HOLDS while the pipeline is frozen —
                                 // a concurrent D-cache refill must not let rdata pair the
                                 // fetch-ahead pc with the still-held IF/ID pc
    output reg [31:0] rdata,     // registered read — instr_bram-compatible on hits
    output        miss,          // 1 = refill in flight or miss pending → freeze pipeline

    // ---- memory interface (1 word per request/ack) ----
    output reg [31:0] mem_addr,
    input  [31:0] mem_rdata,
    output reg    mem_read_req,
    input         mem_read_ack
);
    localparam TAG_W   = 20;
    localparam WORDS   = 8;
    localparam LINES   = 128;

    wire [6:0]  index = pc[11:5];
    wire [2:0]  word  = pc[4:2];
    wire [19:0] tag   = pc[31:12];

    reg [TAG_W-1:0] tag_mem [0:LINES-1];
    reg             valid   [0:LINES-1];
    reg [31:0]      data    [0:LINES-1][0:WORDS-1];

    localparam IDLE = 2'b00, MISS = 2'b01;
    reg [1:0] state;

    reg [6:0]  miss_index;
    reg [19:0] miss_tag;
    reg [2:0]  fill_cnt;
    integer    vi;

    wire line_hit = valid[index] && (tag_mem[index] == tag);
    // A miss is signalled combinationally while still in IDLE so the freeze
    // lands on the SAME cycle the fetch would otherwise advance IF/ID.
    wire miss_in_idle = (state == IDLE) && ~line_hit;
    assign miss = miss_in_idle | (state == MISS);

    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE;
            rdata <= 32'b0;
            mem_read_req <= 1'b0;
            for (vi = 0; vi < LINES; vi = vi + 1) valid[vi] <= 1'b0;
        end else begin
            mem_read_req <= 1'b0;     // pulse
            case (state)
                IDLE: begin
                    if (line_hit) begin
                        // present the instruction only while the fetch is actually
                        // consuming; hold rdata across any freeze (miss-detect stays
                        // unconditional so a stall on a cached line never refills)
                        if (fetch_en) rdata <= data[index][word];
                    end else begin
                        // cold miss: latch pc's line and refill it
                        state      <= MISS;
                        miss_index <= index;
                        miss_tag   <= tag;
                        fill_cnt   <= 3'b0;
                        mem_addr   <= {pc[31:5], 5'b0};
                        mem_read_req <= 1'b1;
                    end
                end
                MISS: begin
                    if (mem_read_ack) begin
                        data[miss_index][fill_cnt] <= mem_rdata;
                        if (fill_cnt == 3'd7) begin
                            tag_mem[miss_index] <= miss_tag;
                            valid[miss_index]   <= 1'b1;
                            state <= IDLE;      // rdata stays as-was; the fetch
                                                // resumes via a normal IDLE read
                        end else begin
                            fill_cnt  <= fill_cnt + 1'b1;
                            mem_addr  <= {miss_tag, miss_index, fill_cnt + 1'b1, 2'b0};
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
