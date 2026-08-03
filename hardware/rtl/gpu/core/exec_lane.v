// Omni-RISC APU — GPU Core: exec_lane
//
// One SIMT execution lane (4 sub-lanes). Combines regfile, ALU, LSU, scratchpad.
// All 4 sub-lanes execute the same instruction in lockstep.
// Interface matches gpu_top expectations: packed 128-bit operands/results.
module exec_lane (
    input         clk,
    input         reset,

    // instruction
    input  [15:0] instr,           // 16-bit SIMT ISA
    input         instr_valid,     // instruction is valid this cycle

    // register file ports
    input  [2:0]  rs1_addr,
    input  [2:0]  rs2_addr,
    input  [2:0]  rd_addr,
    input         rd_write_en,
    input  [127:0] rd_data,

    // scratchpad
    input  [31:0] sp_raddr,
    input  [31:0] sp_waddr,
    input  [127:0] sp_wdata,
    input         sp_write_en,
    output [127:0] sp_rdata,

    // outputs
    output [127:0] alu_result,
    output [31:0]  lsu_addr,
    output [127:0] ld_data_out,
    output [127:0] rs2_out        // store data (rs2), consumed at warp level
);

    // lane-level decode: instr[3:0] is the alu_op (gpu_top drives the decoded
    // alu_op here; tb_exec_lane drives it raw). Addressing is register-based —
    // kernels compute effective addresses in registers, so no lane offset.
    wire [3:0]  alu_op = instr[3:0];
    wire [15:0] lsu_offset = 16'b0;

    // register file
    wire [127:0] rs1_data, rs2_data;
    gpu_regfile u_regfile (
        .clk(clk),
        .reset(reset),
        .rs1_addr(rs1_addr),
        .rs2_addr(rs2_addr),
        .rs1_data(rs1_data),
        .rs2_data(rs2_data),
        .rd_addr(rd_addr),
        .rd_data(rd_data),
        .rd_write_en(rd_write_en)
    );

    // ALU
    gpu_alu u_alu (
        .a(rs1_data),
        .b(rs2_data),
        .alu_op(alu_op),
        .r(alu_result)
    );

    // LSU
    gpu_lsu u_lsu (
        .base(rs1_data),
        .offset(lsu_offset),
        .store_data(rs2_data),
        .addr(lsu_addr),
        .ld_data(sp_rdata),
        .ld_data_out(ld_data_out)
    );

    // scratchpad
    gpu_scratchpad u_sp (
        .clk(clk),
        .reset(reset),
        .raddr(sp_raddr),
        .rdata(sp_rdata),
        .waddr(sp_waddr),
        .wdata(sp_wdata),
        .write_en(sp_write_en)
    );

    assign rs2_out = rs2_data;

endmodule
