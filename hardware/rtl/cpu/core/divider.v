// Omni-RISC APU — CPU: divider
//
// RV32M divide/remainder: iterative shift-subtract, 32 cycles per op,
// start/busy/done handshake with exec_stage. Implements RISC-V semantics
// for the edge cases: divide-by-zero returns -1 (DIV) / dividend (REM),
// overflow (MIN / -1) returns the dividend without trapping.
module divider (
  input             clk,
  input             reset,
  input             start,        
  input      [31:0] operand_a,    
  input      [31:0] operand_b,    
  input      [2:0]  funct3,       // 100 DIV, 101 DIVU, 110 REM, 111 REMU
  output reg [31:0] result,       
  output reg        busy,         // high while computing
  output reg        done          // 1-cycle pulse, result valid this cycle
);

    // 3-State Machine
    localparam IDLE   = 2'b00;
    localparam DIVIDE = 2'b01;
    localparam FINISH = 2'b10;
    reg [1:0] state;

    reg [32:0] rem;     // 33-bit: holds 2*rem + next_bit before conditional subtract
    reg [31:0] quo;     // 32-bit: quotient accumulator
    reg [31:0] divd;    // 32-bit: dividend feed, MSB-first

    reg [5:0]  count;
    reg sign_quo_reg, sign_rem_reg, is_rem_reg;

    wire is_signed = ~funct3[0]; 
    wire a_neg     = is_signed & operand_a[31];
    wire b_neg     = is_signed & operand_b[31];
    reg [31:0] b_mag_reg;
    wire [31:0] a_mag = a_neg ? (~operand_a + 1) : operand_a;
    wire [31:0] b_mag = b_neg ? (~operand_b + 1) : operand_b;

    wire [5:0]  active_zeros  = clz(a_mag);
    wire [5:0]  active_bits   = 6'd32 - active_zeros;
    wire [31:0] a_pre_shifted = a_mag << active_zeros;

    always @(posedge clk) begin
        if (reset) begin
            state  <= IDLE;
            busy   <= 1'b0;
            done   <= 1'b0;
            count  <= 0;
            rem    <= 33'b0;
            quo    <= 32'b0;
            divd   <= 32'b0;
            result <= 32'b0;
        end else begin
            case(state)
                IDLE: begin
                    done <= 1'b0; // Always clear the done pulse
                    
                    if (start) begin
                        busy <= 1'b1; // Default to busy, fast-paths will overwrite this
                        
                        // Latch control signs for the normal path
                        sign_quo_reg <= a_neg ^ b_neg; 
                        sign_rem_reg <= a_neg;
                        is_rem_reg   <= funct3[1];
                        
                        // --- FAST-PATH ASSIGNMENT ---
                        if (operand_b == 32'b0) begin
                            result <= funct3[1] ? operand_a : 32'hFFFF_FFFF;
                            done   <= 1'b1;
                            busy   <= 1'b0;
                        end 
                        else if (is_signed && operand_a == 32'h8000_0000 && operand_b == 32'hFFFF_FFFF) begin
                            result <= funct3[1] ? 32'b0 : 32'h8000_0000;
                            done   <= 1'b1;
                            busy   <= 1'b0;
                        end 
                        else if (b_mag > a_mag) begin
                            result <= funct3[1] ? operand_a : 32'b0;
                            done   <= 1'b1;
                            busy   <= 1'b0;
                        end 
                        else begin
                            rem   <= 33'b0;
                            quo   <= 32'b0;
                            divd  <= a_pre_shifted;
                            count <= active_bits;
                            state <= DIVIDE;
                            b_mag_reg <= b_mag;
                        end
                    end
                end
                
                DIVIDE: begin
                    if (count > 0) begin
                        // 33-bit compare AND subtract (prevents underflow)
                        if ( {rem[31:0], divd[31]} >= {1'b0, b_mag_reg} ) begin
                            rem <= {rem[31:0], divd[31]} - {1'b0, b_mag_reg};
                            quo <= {quo[30:0], 1'b1};
                        end else begin
                            rem <= {rem[31:0], divd[31]};
                            quo <= {quo[30:0], 1'b0};
                        end
                        
                        divd  <= {divd[30:0], 1'b0};
                        count <= count - 1;
                        
                        if (count == 6'd1) begin
                            state <= FINISH; // Move to formatting stage
                        end
                    end
                end

                FINISH: begin
                    if (is_rem_reg) begin
                        result <= sign_rem_reg ? (~rem[31:0] + 1) : rem[31:0];
                    end else begin
                        result <= sign_quo_reg ? (~quo + 1) : quo;
                    end
                    
                    done  <= 1'b1;
                    busy  <= 1'b0;
                    state <= IDLE;
                end
                
                default: state <= IDLE;
            endcase 
        end
    end

    function [5:0] clz(input [31:0] val);
        reg c4, c3, c2, c1, c0;
        reg [15:0] s16; reg [7:0] s8; reg [3:0] s4; reg [1:0] s2;
        begin
            if (val == 0) begin
                clz = 6'd32;
            end else begin
                c4 = (val[31:16] == 16'b0); s16 = c4 ? val[15:0] : val[31:16];
                c3 = (s16[15:8]  == 8'b0);  s8  = c3 ? s16[7:0]  : s16[15:8];
                c2 = (s8[7:4]    == 4'b0);  s4  = c2 ? s8[3:0]   : s8[7:4];
                c1 = (s4[3:2]    == 2'b0);  s2  = c1 ? s4[1:0]   : s4[3:2];
                c0 = (s2[1]      == 1'b0);
                clz = {1'b0, c4, c3, c2, c1, c0};
            end
        end
    endfunction

endmodule
