// Omni-RISC APU — GPU Core: gpu_regfile
//
// SIMT register file: 8 registers × 4 lanes × 32 bits. Each register holds a
// 128-bit vector (lane0=[31:0] ... lane3=[127:96]). 2 read ports + 1 write
// port, like the CPU regfile but vector-wide. r0 is hardwired zero.
module gpu_regfile (
    input         clk,
    input         reset,

    // read ports (register index)
    input  [2:0]  rs1_addr,
    input  [2:0]  rs2_addr,
    output [127:0] rs1_data,
    output [127:0] rs2_data,

    // write port
    input  [2:0]  rd_addr,
    input  [127:0] rd_data,
    input         rd_write_en
);
    reg [127:0] regs [0:7];

    always @(posedge clk) begin
        if (rd_write_en && rd_addr != 3'b0) begin
            regs[rd_addr] <= rd_data;
        end
    end

    assign rs1_data = (rs1_addr == 3'b0) ? 128'b0 : regs[rs1_addr];
    assign rs2_data = (rs2_addr == 3'b0) ? 128'b0 : regs[rs2_addr];
endmodule
