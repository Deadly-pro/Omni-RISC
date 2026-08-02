// Omni-RISC APU — GPU Core: gpu_lsu
//
// 4-lane SIMT load/store unit. Computes each lane's effective address
// (base[lane] + sign-extended offset) and presents it to the scratchpad as a
// packed 32-bit address bus. Passes per-lane store data / load data through.
module gpu_lsu (
    input  [127:0] base,       // per-lane base address (lane0 in [31:0])
    input  [15:0]  offset,     // sign-extended immediate
    input  [127:0] store_data, // per-lane store data
    output [31:0]  addr,       // packed per-lane word addresses for scratchpad
    input  [127:0] ld_data,    // loaded data (passthrough from scratchpad)
    output [127:0] ld_data_out // forwarded load data
);
    wire [31:0] off = {{16{offset[15]}}, offset};  // sign-extend to 32

    assign addr[31:0]  = base[31:0]  + off;
    assign addr[63:32] = base[63:32] + off;
    assign addr[95:64] = base[95:64] + off;
    assign addr[127:96] = base[127:96] + off;
    assign ld_data_out = ld_data;
    // store_data is passed to the scratchpad write port at the warp level
endmodule
