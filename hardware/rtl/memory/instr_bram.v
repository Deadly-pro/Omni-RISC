// Omni-RISC APU — Memory: instr_bram
// TODO: Implement
module instr_bram #(
    parameter MemFile="program.hex"
) (
    input clk,
    input en,
    input [31:0] pc,
    output reg [31:0] rdata
);
reg [31:0] mem[0:1023];
initial begin
    $readmemh(MemFile,mem);
end

always @(posedge clk) begin
    if(en)rdata<=mem[pc[11:2]];
end
endmodule
