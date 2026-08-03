// Omni-RISC APU — GPU Core: gpu_lsu
//
// 4-lane SIMT load/store unit. Computes each lane's effective address
// (base[lane] + sign-extended offset) and presents it to the scratchpad as a
// packed 32-bit address bus (8 bits per lane). Passes per-lane store data /
// load data through.
module gpu_lsu (
    input  [127:0] base,       // per-lane base address (lane0 in [31:0])
    input  [15:0]  offset,     // sign-extended immediate
    input  [127:0] store_data, // per-lane store data
    output [31:0]  addr,       // packed per-lane word addresses for scratchpad (8 bits per lane)
    input  [127:0] ld_data,    // loaded data (passthrough from scratchpad)
    output [127:0] ld_data_out // forwarded load data
);
    wire [31:0] off = {{16{offset[15]}}, offset};  // sign-extend to 32

    wire [31:0] eff0 = base[31:0]   + off;
    wire [31:0] eff1 = base[63:32]  + off;
    wire [31:0] eff2 = base[95:64]  + off;
    wire [31:0] eff3 = base[127:96] + off;

    // Pack 8-bit word addresses per lane for scratchpad
    assign addr[7:0]   = eff0[7:0];
    assign addr[15:8]  = eff1[7:0];
    assign addr[23:16] = eff2[7:0];
    assign addr[31:24] = eff3[7:0];

    assign ld_data_out = ld_data;
    // store_data is passed to the scratchpad write port at the warp level
endmodule
