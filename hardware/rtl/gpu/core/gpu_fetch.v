// Omni-RISC APU — GPU Core: gpu_fetch
//
// Instruction fetch for GPU warps. Simple instruction memory (imem) with
// single-cycle read latency. Warp scheduler provides warp_id and PC.
module gpu_fetch #(
    parameter IMEM_DEPTH = 1024,
    parameter IMEM_FILE = ""
) (
    input         clk,
    input         reset,

    input  [1:0]  warp_id,
    input  [31:0] pc,
    output [15:0] instr,
    output        valid
);

    reg [15:0] imem [0:IMEM_DEPTH-1] /*verilator public*/;

    initial begin
        if (IMEM_FILE != "") $readmemh(IMEM_FILE, imem);
    end

    wire [9:0] idx = pc[10:1];  // word address (16-bit instr = 2 bytes)
    reg [15:0] instr_reg;
    reg        valid_reg;

    always @(posedge clk) begin
        if (reset) begin
            instr_reg <= 16'b0;
            valid_reg <= 1'b0;
        end else begin
            instr_reg <= imem[idx];
            valid_reg <= 1'b1;
        end
    end

    assign instr = instr_reg;
    assign valid = valid_reg;

endmodule
