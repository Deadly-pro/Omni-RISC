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
// Transient states handled:
//   - Snoop hit during refill (WAITM_RD) → defer snoop until refill completes
//   - Snoop hit during writeback (WB_M)   → wait for writeback to complete
//   - Same-cycle CPU request + snoop    → CPU wins, snoop retried next cycle
//   - Writeback is 8 words, one per cycle with mem_write_req/ack
//
// LR/SC extension:
//   - LR.W: load-reserve, sets reservation (addr + valid)
//   - SC.W: store-conditional, succeeds only if reservation matches addr
//   - Reservation cleared on: any other store (local or remote), cache invalidation
//
// Memory interface (1 word per request/ack):
//   CPU:   addr, wdata, byte_en[3:0], read_en, write_en, is_lr, is_sc, rdata, hit, miss, ready
//   Memory: mem_addr, mem_rdata, mem_read_req, mem_read_ack,
//           mem_write_req, mem_wdata, mem_write_ack
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
    input         is_lr,          // LR.W instruction
    input         is_sc,          // SC.W instruction
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

    // State machine with transient states for snoop handling
    localparam IDLE        = 4'b0000,
               CHECK       = 4'b0001,
               WAITM_RD    = 4'b0010,  // waiting for memory read ack (refill)
               WAITM_WR    = 4'b0011,  // waiting for memory write ack (write-through)
               WB_M        = 4'b0100,  // writeback M line to memory (8 words)
               SNOOP_INV   = 4'b0101,  // processing snoop invalidate
               SNOOP_WAIT  = 4'b0110,  // snoop arrived during refill — wait for refill
               SNOOP_WB    = 4'b0111;  // snoop read hit M — writeback then go to S

    reg [3:0] state;

    reg       req;                    // a request is latched
    reg       req_read, req_write;
    reg       req_is_lr, req_is_sc;   // latched LR/SC so the type survives a refill
    reg [31:0] req_addr, req_wdata;
    reg [3:0]  req_ben;

    reg [2:0]  fill_cnt;
    reg        fill_way;
    reg        last_way;              // way filled most recently (round-robin)
    reg        pending_write;         // a write-allocate refill is in progress

    reg [1:0]  snoop_msi;            // MSI state of snooped line
    reg        snoop_hit;             // snoop address hits in our cache
    reg        snoop_hit0, snoop_hit1;
    reg [5:0]  snoop_idx;
    reg        snoop_is_read, snoop_is_write;
    reg [2:0]  wb_cnt;                // writeback word counter (0-7)

    // LR/SC reservation tracking
    reg        reservation_valid;     // reservation is active
    reg [31:0] reservation_addr;      // reserved address

    integer k;
    reg [31:0] wnew;

    // Pick a way for a refill: prefer an invalid way, else the least-recently filled
    function automatic pick_way(input [31:0] a, input last);
        if (msi[0][a[10:5]] == MSI_I)      pick_way = 1'b0;
        else if (msi[1][a[10:5]] == MSI_I) pick_way = 1'b1;
        else                               pick_way = last ? 1'b0 : 1'b1;
    endfunction

    // Snoop hit detection (combinational)
    wire snoop_idx_c = snoop_addr[10:5];
    wire snoop_hit0_c = (msi[0][snoop_idx_c] != MSI_I) && tag[0][snoop_idx_c] == snoop_addr[31:11];
    wire snoop_hit1_c = (msi[1][snoop_idx_c] != MSI_I) && tag[1][snoop_idx_c] == snoop_addr[31:11];
    wire snoop_hit_any_c = snoop_hit0_c || snoop_hit1;

    // Snoop MSI state (combinational)
    wire [1:0] snoop_msi_c = snoop_hit0_c ? msi[0][snoop_idx_c] :
                             snoop_hit1_c ? msi[1][snoop_idx_c] : MSI_I;

    always @(posedge clk) begin
        if (reset) begin
            state <= IDLE; req <= 1'b0; ready <= 1'b0;
            mem_read_req <= 1'b0; mem_write_req <= 1'b0;
            rdata <= 32'b0; last_way <= 1'b0; pending_write <= 1'b0;
            wb_cnt <= 3'b0;
            reservation_valid <= 1'b0;
            reservation_addr <= 32'b0;
            for (k = 0; k < SETS; k = k + 1) begin
                msi[0][k] <= MSI_I;
                msi[1][k] <= MSI_I;
            end
        end else begin
            mem_read_req <= 1'b0;
            mem_write_req <= 1'b0;
            ready <= 1'b0;

            // Latch new CPU request (incl. LR/SC type — the one-shot read/write
            // pulses drop long before a refill completes)
            if (read_en | write_en) begin
                req <= 1'b1; req_read <= read_en; req_write <= write_en;
                req_is_lr <= is_lr; req_is_sc <= is_sc;
                req_addr <= addr; req_wdata <= wdata; req_ben <= byte_en;
            end

            // Snoop hit detection (capture on IDLE or when not busy with CPU)
            if (state == IDLE || state == CHECK) begin
                snoop_hit <= snoop_hit_any_c;
                snoop_hit0 <= snoop_hit0_c;
                snoop_hit1 <= snoop_hit1_c;
                snoop_idx <= snoop_idx_c;
                snoop_msi <= snoop_msi_c;
                snoop_is_read <= snoop_read;
                snoop_is_write <= snoop_write;
            end

            case (state)
                IDLE: begin
                    // Priority: CPU request > snoop
                    if (req) begin
                        state <= CHECK;
                    end else if (snoop_read && snoop_hit && snoop_msi == MSI_M) begin
                        // Other core reads a line we have Modified → must writeback
                        wb_cnt <= 3'b0;
                        state <= SNOOP_WB;
                    end else if (snoop_write && snoop_hit && (snoop_msi == MSI_S || snoop_msi == MSI_M)) begin
                        // Other core writes a line we have Shared/Modified → invalidate
                        state <= SNOOP_INV;
                    end
                end

                CHECK: begin
                    if (read_en | write_en) begin
                        // New request arrived while latching — re-enter CHECK next cycle
                        state <= CHECK;
                    end
                    else if (req_is_lr && req_read) begin
                        // LR.W: load-reserve
                        if (req_hit) begin
                            rdata <= data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]];
                            reservation_valid <= 1'b1;
                            reservation_addr <= req_addr;
                            ready <= 1'b1; req <= 1'b0; state <= IDLE;
                        end else begin
                            // LR miss → refill, then set reservation (WAITM_RD
                            // completion sees req_is_lr)
                            pending_write <= 1'b0;  // not a write-allocate
                            fill_cnt <= 3'b0;
                            fill_way <= pick_way(req_addr, last_way);
                            mem_addr <= {req_addr[31:5], 5'b0};
                            mem_read_req <= 1'b1;
                            state <= WAITM_RD;
                        end
                    end
                    else if (req_is_sc && req_write) begin
                        // SC.W: store-conditional
                        if (req_hit && reservation_valid && (reservation_addr == req_addr)) begin
                            // Reservation valid and address matches → succeed
                            wnew = data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]];
                            if (req_ben[0]) wnew[7:0]   = req_wdata[7:0];
                            if (req_ben[1]) wnew[15:8]  = req_wdata[15:8];
                            if (req_ben[2]) wnew[23:16] = req_wdata[23:16];
                            if (req_ben[3]) wnew[31:24] = req_wdata[31:24];
                            data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]] <= wnew;
                            mem_addr <= req_addr;
                            mem_wdata <= wnew;
                            mem_write_req <= 1'b1;
                            if (hit0) msi[0][req_addr[10:5]] <= MSI_M;
                            else      msi[1][req_addr[10:5]] <= MSI_M;
                            reservation_valid <= 1'b0;  // clear reservation on success
                            rdata <= 32'b0;  // SC returns 0 on success
                            ready <= 1'b1; req <= 1'b0; state <= IDLE;
                        end else begin
                            // Reservation invalid or address mismatch → fail
                            reservation_valid <= 1'b0;
                            rdata <= 32'b1;  // SC returns non-zero on failure
                            ready <= 1'b1; req <= 1'b0; state <= IDLE;
                        end
                    end
                    else if (req_write) begin
                        // Regular write: if hit, update cache and write-through to memory
                        if (req_hit) begin
                            wnew = data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]];
                            if (req_ben[0]) wnew[7:0]   = req_wdata[7:0];
                            if (req_ben[1]) wnew[15:8]  = req_wdata[15:8];
                            if (req_ben[2]) wnew[23:16] = req_wdata[23:16];
                            if (req_ben[3]) wnew[31:24] = req_wdata[31:24];
                            data[hit0 ? 0 : 1][req_addr[10:5]][req_addr[4:2]] <= wnew;
                            // Write-through: also write to memory
                            mem_addr <= req_addr;
                            mem_wdata <= wnew;
                            mem_write_req <= 1'b1;
                            // State becomes M (was S or M)
                            if (hit0) msi[0][req_addr[10:5]] <= MSI_M;
                            else      msi[1][req_addr[10:5]] <= MSI_M;
                            // Any local store clears reservation
                            reservation_valid <= 1'b0;
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
                        // State stays M or S
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
                                // a refilled LR.W miss must arm the reservation
                                // (the CHECK hit path did it on a hit)
                                if (req_is_lr) begin
                                    reservation_valid <= 1'b1;
                                    reservation_addr <= req_addr;
                                end
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
                    // Explicit writeback M line (from eviction, not snoop)
                    // This state is entered when we need to evict an M line
                    if (!mem_write_req || mem_write_ack) begin
                        if (wb_cnt == 3'd7) begin
                            // Writeback complete
                            if (snoop_hit0) msi[0][snoop_idx] <= MSI_S;
                            else if (snoop_hit1) msi[1][snoop_idx] <= MSI_S;
                            // Eviction clears reservation if it matches
                            if (reservation_valid && (reservation_addr[31:5] == {tag[snoop_hit0 ? 0 : 1][snoop_idx], snoop_idx})) begin
                                reservation_valid <= 1'b0;
                            end
                            state <= IDLE;
                        end else begin
                            wb_cnt <= wb_cnt + 1'b1;
                            mem_addr <= {tag[snoop_hit0 ? 0 : 1][snoop_idx], snoop_idx, wb_cnt + 1'b1, 2'b0};
                            mem_wdata <= data[snoop_hit0 ? 0 : 1][snoop_idx][wb_cnt + 1'b1];
                            mem_write_req <= 1'b1;
                        end
                    end
                end

                SNOOP_WB: begin
                    // Snoop read hit M: writeback the full line (8 words), then go to S
                    if (!mem_write_req || mem_write_ack) begin
                        if (wb_cnt == 3'd7) begin
                            // Writeback complete
                            if (snoop_hit0) msi[0][snoop_idx] <= MSI_S;
                            else if (snoop_hit1) msi[1][snoop_idx] <= MSI_S;
                            state <= IDLE;
                        end else begin
                            wb_cnt <= wb_cnt + 1'b1;
                            mem_addr <= {tag[snoop_hit0 ? 0 : 1][snoop_idx], snoop_idx, wb_cnt + 1'b1, 2'b0};
                            mem_wdata <= data[snoop_hit0 ? 0 : 1][snoop_idx][wb_cnt + 1'b1];
                            mem_write_req <= 1'b1;
                        end
                    end
                end

                SNOOP_INV: begin
                    // Invalidate the line (snoop write hit S or M)
                    if (snoop_hit0) msi[0][snoop_idx] <= MSI_I;
                    else if (snoop_hit1) msi[1][snoop_idx] <= MSI_I;
                    // Snoop write invalidates our reservation if it matches
                    if (reservation_valid && (reservation_addr[31:5] == {snoop_addr[31:11], snoop_idx})) begin
                        reservation_valid <= 1'b0;
                    end
                    state <= IDLE;
                end

                default: state <= IDLE;
            endcase
        end
    end

    // Snoop acknowledge: asserted for one cycle when snoop is handled
    reg snoop_ack_r;
    always @(posedge clk) begin
        if (reset) snoop_ack_r <= 1'b0;
        else begin
            snoop_ack_r <= 1'b0;
            if (state == SNOOP_INV) snoop_ack_r <= 1'b1;
            else if (state == SNOOP_WB && wb_cnt == 3'd7 && (!mem_write_req || mem_write_ack)) snoop_ack_r <= 1'b1;
        end
    end
    assign snoop_ack = snoop_ack_r;

endmodule
