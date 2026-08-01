// Omni-RISC APU — L1 data cache with MSI snooping
//
// 4KB, 2-way set-associative, write-through, write-allocate (for MSI, allocate on write miss).
// 32-byte lines (8 words), 64 sets.
//   index = addr[10:5], tag = addr[31:11], word = addr[4:2]
//
// MSI states per line:
//   M (Modified)  = 2'b11  — dirty, only this core has it
//   S (Shared)    = 2'b10  — clean, may be in other core(s)
//   I (Invalid)   = 2'b00  — not present
//   (no Exclusive state — write-through means no dirty on clean line)
//
// Snooping: each core's D$ monitors the shared bus for the other core's transactions.
// On a bus read (other core reads): if we have M → write back + transition to S.
// On a bus write (other core writes): if we have S or M → invalidate (→ I).
//
// Memory interface (1 word per request/ack):
//   CPU:   addr, wdata, byte_en[3:0], read_en, write_en, rdata, hit, miss, ready
//   Memory: mem_addr, mem_rdata, mem_read_req, mem_read_ack
//   Snoop:  snoop_addr, snoop_read, snoop_write, snoop_ack (to bus)
module l1_dcache_msi (
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
    input         mem_read_ack,
    output reg    mem_write_req,
    output reg [31:0] mem_wdata,
    input         mem_write_ack,

    // ---- snoop interface (from bus, for other core's transactions) ----
    input  [31:0] snoop_addr,
    input         snoop_read,   // other core doing a read
    input         snoop_write,  // other core doing a write
    output        snoop_ack     // we've handled the snoop (invalidate/writeback)
);

    localparam INDEX_W = 6;
    localparam TAG_W   = 21;
    localparam WORDS   = 8;
    localparam SETS    = 64;

    // MSI encoding
    localparam MSI_I = 2'b00;
    localparam MSI_S = 2'b10;
    localparam MSI_M = 2'b11;

    // MMIO bypass: addresses >= 0x10000000 are never cached
    wire mmio = (addr[31:28] != 4'b0);

    // Tag array
    reg [TAG_W-1:0] tag  [0:1][0:SETS-1];
    // MSI state array (2 bits per line)
    reg [1:0]       msi  [0:1][0:SETS-1];
    // Data array
    reg [31:0]      data [0:1][0:SETS-1][0:WORDS-1];

    // Current request hit/miss (combinational)
    wire hit0 = (msi[0][addr[10:5]] != MSI_I) && tag[0][addr[10:5]] == addr[31:11];
    wire hit1 = (msi[1][addr[10:5]] != MSI_I) && tag[1][addr[10:5]] == addr[31:11];
    wire req_hit = (hit0 || hit1) && ~mmio;
    wire req_miss = (read_en | write_en) & ~req_hit & ~mmio;

    assign hit  = req_hit;
    assign miss = req_miss;

    // Valid is derived from MSI != I
    wire [1:0] valid0 = (msi[0][addr[10:5]] != MSI_I);
    wire [1:0] valid1 = (msi[1][addr[10:5]] != MSI_I);
    wire valid0_hit = (msi[0][addr[10:5]] != MSI_I) && tag[0][addr[10:5]] == addr[31:11];
    wire valid1_hit = (msi[1][addr[10:5]] != MSI_I) && tag[1][addr[10:5]] == addr[31:11];

    localparam IDLE      = 3'b000,
               CHECK     = 3'b001,
               WAITM_RD  = 3'b010,  // waiting for memory read ack
               WAITM_WR  = 3'b011,  // waiting for memory write ack
               WB_M      = 3'b100,  // writeback M line to memory
               SNOOP_INV = 3'b101;  // processing snoop invalidate

    reg [2:0] state;

    reg       req;                    // a request is latched
    reg       req_read, req_write;
    reg [31:0] req_addr, req_wdata;
    reg [3:0]  req_ben;

    reg [2:0]  fill_cnt;
    reg        fill_way;
    reg        last_way;              // way filled most recently (round-robin)
    reg        pending_write;         // a write-allocate refill is in progress

    reg [1:0]  snoop_msi;            // MSI state of snooped line
    reg        snoop_hit;             // snoop address hits in our cache

    integer k;
    reg [31:0] wnew;

    // Pick a way for a refill: prefer an invalid way, else the least-recently filled
    function automatic pick_way(input [31:0] a, input last);
        if (msi[0][a[10:5]] == MSI_I)      pick_way = 1'b0;
        else if (msi[1][a[10:5]] == MSI_I) pick_way = 1'b1;
        else                               pick_way = last ? 1'b0 : 1'b1;
    endfunction

    // Snoop hit detection (combinational)
    wire snoop_idx = snoop_addr[10:5];
    wire snoop_hit0 = (msi[0][snoop_idx] != MSI_I) && tag[0][snoop_idx] == snoop_addr[31:11];
    wire snoop_hit1 = (msi[1][snoop_idx] != MSI_I) && tag[1][snoop_idx] == snoop_addr[31:11];
    wire snoop_hit_any = snoop_hit0 || snoop_hit1;

    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE; req <= 1'b0; ready <= 1'b0;
            mem_read_req <= 1'b0; mem_write_req <= 1'b0;
            rdata <= 32'b0; last_way <= 1'b0; pending_write <= 1'b0;
            for (k = 0; k < SETS; k = k + 1) begin
                msi[0][k] <= MSI_I;
                msi[1][k] <= MSI_I;
            end
        end else begin
            mem_read_req <= 1'b0;
            mem_write_req <= 1'b0;
            ready <= 1'b0;

            // Latch new CPU request
            if (read_en | write_en) begin
                req <= 1'b1; req_read <= read_en; req_write <= write_en;
                req_addr <= addr; req_wdata <= wdata; req_ben <= byte_en;
            end

            // Snoop hit detection
            snoop_hit <= snoop_hit_any;
            if (snoop_hit0) snoop_msi <= msi[0][snoop_idx];
            else if (snoop_hit1) snoop_msi <= msi[1][snoop_idx];

            case (state)
                IDLE: begin
                    if (req) state <= CHECK;
                    else if (snoop_read && snoop_hit_any && snoop_msi == MSI_M) state <= WB_M;
                    else if (snoop_write && snoop_hit_any && (snoop_msi == MSI_S || snoop_msi == MSI_M)) state <= SNOOP_INV;
                end

                CHECK: begin
                    if (read_en | write_en) begin
                        state <= CHECK;
                    end
                    else if (req_write) begin
                        // Write: if hit, update cache and write-through to memory
                        if (req_hit) begin
                            wnew = data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]];
                            if (req_ben[0]) wnew[7:0]   = req_wdata[7:0];
                            if (req_ben[1]) wnew[15:8]  = req_wdata[15:8];
                            if (req_ben[2]) wnew[23:16] = req_wdata[23:16];
                            if (req_ben[3]) wnew[31:24] = req_wdata[31:24];
                            data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]] <= wnew;
                            // Write-through: also write to memory (M state stays M)
                            mem_addr <= req_addr;
                            mem_wdata <= {wnew[31:0]}; // simplified: write whole word
                            mem_write_req <= 1'b1;
                            // State stays M (already modified) or becomes M if was S
                            if (hit0) msi[0][req_addr[10:5]] <= MSI_M;
                            else      msi[1][req_addr[10:5]] <= MSI_M;
                            ready <= 1'b1; req <= 1'b0; state <= IDLE;
                        end else begin
                            // Write miss → write-allocate: refill line, then apply
                            pending_write <= 1'b1;
                            fill_cnt <= 3'b0;
                            fill_way <= pick_way(req_addr, last_way);
                            mem_addr <= {req_addr[31:5], 5'b0};
                            mem_read_req <= 1'b1;
                            state <= WAITM_RD;
                        end
                    end
                    else if (req_hit) begin
                        // Read hit
                        rdata <= data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]];
                        // If was M, stays M; if was S, stays S
                        ready <= 1'b1; req <= 1'b0; state <= IDLE;
                    end
                    else begin
                        // Read miss → refill the line (8 words)
                        fill_cnt <= 3'b0;
                        fill_way <= pick_way(req_addr, last_way);
                        mem_addr <= {req_addr[31:5], 5'b0};
                        mem_read_req <= 1'b1;
                        state <= WAITM_RD;
                    end
                end

                WAITM_RD: begin
                    if (mem_read_ack) begin
                        data[fill_way][req_addr[10:5]][fill_cnt] <= mem_rdata;
                        if (fill_cnt == 3'd7) begin
                            tag[fill_way][req_addr[10:5]] <= req_addr[31:11];
                            msi[fill_way][req_addr[10:5]] <= MSI_S;  // fresh from memory = Shared
                            last_way <= fill_way;
                            if (pending_write) begin
                                // Write-allocate: apply the pending write
                                pending_write <= 1'b0;
                                wnew = data[fill_way][req_addr[10:5]][req_addr[4:2]];
                                if (req_ben[0]) wnew[7:0]   = req_wdata[7:0];
                                if (req_ben[1]) wnew[15:8]  = req_wdata[15:8];
                                if (req_ben[2]) wnew[23:16] = req_wdata[23:16];
                                if (req_ben[3]) wnew[31:24] = req_wdata[31:24];
                                data[fill_way][req_addr[10:5]][req_addr[4:2]] <= wnew;
                                mem_addr <= req_addr;
                                mem_wdata <= wnew;
                                mem_write_req <= 1'b1;  // write-through
                                msi[fill_way][req_addr[10:5]] <= MSI_M;
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

                WB_M: begin
                    // Writeback M line to memory, then go to S
                    // We need to write all 8 words of the line
                    // For simplicity, write back the whole line word by word
                    // (A real implementation would have a burst write)
                    if (!mem_write_req || mem_write_ack) begin
                        // This is simplified - a full line writeback would need a counter
                        if (snoop_hit0) msi[0][snoop_idx] <= MSI_S;
                        else if (snoop_hit1) msi[1][snoop_idx] <= MSI_S;
                        state <= IDLE;
                    end
                end

                SNOOP_INV: begin
                    // Invalidate the line
                    if (snoop_hit0) msi[0][snoop_idx] <= MSI_I;
                    else if (snoop_hit1) msi[1][snoop_idx] <= MSI_I;
                    state <= IDLE;
                end

                default: state <= IDLE;
            endcase
        end
    end

    // Snoop acknowledge (combinational for now)
    assign snoop_ack = (state == WB_M) || (state == SNOOP_INV);

endmodule
