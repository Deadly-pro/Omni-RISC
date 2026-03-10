// 5-stage pipelined RV32IMACV core with Decompressor and Bus Interface.
`timescale 1ns / 1ps

module risc_core #(
    parameter [31:0] HART_ID = 32'h00000000
)(
    input  wire        clk,
    input  wire        rst,

    // Instruction Bus Interface (Talks to L1 I-Cache or Bus)
    output wire [31:0] ibus_addr_out,
    output wire        ibus_req_out,
    input  wire [31:0] ibus_data_in,
    input  wire        ibus_ack_in,

    // Data Bus Interface (Talks to L1 D-Cache or Bus)
    output wire [31:0] dbus_addr_out,
    output wire [31:0] dbus_write_data_out,
    output wire        dbus_read_en_out,
    output wire        dbus_write_en_out,
    input  wire [31:0] dbus_read_data_in,
    input  wire        dbus_ack_in
);

    // --- Signal Declarations ---
    wire [31:0] next_pc;
    wire [31:0] curr_pc;
    wire [31:0] pc_plus_4;
    
    // Stall Signals
    wire hazard_stall; // From Hazard Detection Unit (Load-Use)
    wire fetch_stall;  // From I-Bus Latency
    wire mem_stall;    // From D-Bus Latency
    
    // Control logic for pipeline buffers
    wire pipe_en_all  = !(fetch_stall || mem_stall);
    wire if_id_en     = pipe_en_all && !hazard_stall;
    wire id_ex_en     = pipe_en_all;
    wire ex_ma_en     = pipe_en_all;
    wire mem_wb_en    = pipe_en_all;

    wire if_id_clr    = rst || branch_taken;
    wire id_ex_clr    = rst || branch_taken || (hazard_stall && pipe_en_all);
    
    wire branch_taken;
    wire [31:0] branch_target;

    // --- IF Stage ---
    prog_counter pc_reg (
        .clk(clk),
        .rst(rst),
        .pc_in(next_pc),
        .pc_out(curr_pc)
    );

    assign next_pc = (branch_taken)      ? branch_target :
                     (if_id_en)          ? pc_plus_4 :
                     curr_pc;

    wire [31:0] raw_instruction;
    ins_fetch fetch_stage (
        .clk(clk), .rst(rst),
        .pipeline_stall_in(mem_stall || hazard_stall), 
        .fetch_stall_out(fetch_stall),
        .pc_in(curr_pc), .pc_plus_4_out(pc_plus_4),
        .ibus_addr_out(ibus_addr_out), .ibus_req_out(ibus_req_out),
        .ibus_data_in(ibus_data_in), .ibus_ack_in(ibus_ack_in),
        .instruction_out(raw_instruction)
    );

    wire [31:0] decompressed_instruction;
    wire        is_compressed_wire;
    riscv_decompressor decomp (
        .instr_in(raw_instruction),
        .instr_out(decompressed_instruction),
        .is_compressed(is_compressed_wire)
    );

    // IF/ID Buffer
    wire [31:0] id_instruction_in, id_pc_plus_4_in, id_pc_in;
    if_id_buffer if_id (
        .clk(clk), .rst(if_id_clr), .pipeline_stall(!if_id_en),
        .if_instruction_in(decompressed_instruction), .if_pc_plus_4_in(pc_plus_4), .if_pc_in(curr_pc),
        .id_instruction_out(id_instruction_in), .id_pc_plus_4_out(id_pc_plus_4_in), .id_pc_out(id_pc_in)
    );

    // --- ID Stage ---
    // Feedback and inter-stage signals
    wire [4:0]  wb_rd_feedback;
    wire [31:0] wb_write_data_feedback;
    wire        wb_reg_write_feedback;

    wire [4:0]  ex_rd_addr_in;
    wire        ex_mem_read_in, ex_reg_write_in;

    // Decode stage signals
    wire [31:0] id_pc_plus_4_out, id_pc_out_pass, id_read_data1_out, id_read_data2_out, id_immediate_out;
    wire [4:0]  id_rs1_addr_out, id_rs2_addr_out, id_rd_addr_out;
    wire [31:0] id_instruction_debug_out;
    wire        id_mem_read_out, id_mem_write_out, id_reg_write_out, id_mem_to_reg_out, id_alu_src_out, id_branch_out, id_jump_out, id_write_from_pc_out;
    wire [3:0]  id_alu_ctrl_out;

    ins_decode decode_stage (
        .clk(clk), .rst(rst),
        .id_instruction_in(id_instruction_in), .id_pc_plus_4_in(id_pc_plus_4_in), .id_pc_in(id_pc_in),
        .ex_rd_addr_in(ex_rd_addr_in), .ex_mem_read_in(ex_mem_read_in), .ex_reg_write_in(ex_reg_write_in),
        .wb_write_addr_in(wb_rd_feedback), .wb_write_data_in(wb_write_data_feedback), .wb_reg_write_en_in(wb_reg_write_feedback),
        .pipeline_stall_out(hazard_stall),
        .id_pc_plus_4_out(id_pc_plus_4_out), .id_pc_out(id_pc_out_pass), .id_read_data1_out(id_read_data1_out), .id_read_data2_out(id_read_data2_out),
        .id_immediate_out(id_immediate_out), .id_rs1_addr_out(id_rs1_addr_out), .id_rs2_addr_out(id_rs2_addr_out), .id_rd_addr_out(id_rd_addr_out),
        .id_instruction_out(id_instruction_debug_out), .id_mem_read_out(id_mem_read_out), .id_mem_write_out(id_mem_write_out),
        .id_reg_write_out(id_reg_write_out), .id_mem_to_reg_out(id_mem_to_reg_out), .id_alu_src_out(id_alu_src_out), .id_branch_out(id_branch_out),
        .id_jump_out(id_jump_out),
        .id_alu_ctrl_out(id_alu_ctrl_out), .id_write_from_pc_out(id_write_from_pc_out)
    );

    // ID/EX Buffer
    wire [31:0] ex_pc_plus_4_in, ex_pc_in, ex_read_data1_in, ex_read_data2_in, ex_immediate_in, ex_instruction_in;
    wire [4:0]  ex_rs1_addr_in, ex_rs2_addr_in;
    wire        ex_mem_write_in, ex_mem_to_reg_in, ex_alu_src_in, ex_branch_in, ex_jump_in, ex_write_from_pc_in;
    wire [3:0]  ex_alu_ctrl_in;

    id_ex_buffer id_ex (
        .clk(clk), .rst(rst), .en(id_ex_en), .clr(id_ex_clr),
        .id_pc_plus_4_in(id_pc_plus_4_out), .id_pc_in(id_pc_out_pass), .id_read_data1_in(id_read_data1_out), .id_read_data2_in(id_read_data2_out),
        .id_immediate_in(id_immediate_out), .id_rs1_addr_in(id_rs1_addr_out), .id_rs2_addr_in(id_rs2_addr_out), .id_rd_addr_in(id_rd_addr_out),
        .id_instruction_in(id_instruction_debug_out), .id_mem_read_in(id_mem_read_out), .id_mem_write_in(id_mem_write_out),
        .id_reg_write_in(id_reg_write_out), .id_mem_to_reg_in(id_mem_to_reg_out), .id_alu_src_in(id_alu_src_out), .id_branch_in(id_branch_out),
        .id_jump_in(id_jump_out),
        .id_alu_ctrl_in(id_alu_ctrl_out), .id_write_from_pc_in(id_write_from_pc_out),
        .ex_pc_plus_4_out(ex_pc_plus_4_in), .ex_pc_out(ex_pc_in), .ex_read_data1_out(ex_read_data1_in), .ex_read_data2_out(ex_read_data2_in),
        .ex_immediate_out(ex_immediate_in), .ex_rs1_addr_out(ex_rs1_addr_in), .ex_rs2_addr_out(ex_rs2_addr_in), .ex_rd_addr_out(ex_rd_addr_in),
        .ex_instruction_out(ex_instruction_in), .ex_mem_read_out(ex_mem_read_in), .ex_mem_write_out(ex_mem_write_in), .ex_reg_write_out(ex_reg_write_in),
        .ex_mem_to_reg_out(ex_mem_to_reg_in), .ex_alu_src_out(ex_alu_src_in), .ex_branch_out(ex_branch_in), .ex_jump_out(ex_jump_in), .ex_alu_ctrl_out(ex_alu_ctrl_in), .ex_write_from_pc_out(ex_write_from_pc_in)
        );


    // --- EX Stage ---
    wire [31:0] ex_pc_plus_4_out, ex_alu_result_out, ex_read_data2_out;
    wire [4:0]  ex_rd_addr_out;
    wire        ex_mem_read_out, ex_mem_write_out, ex_reg_write_out, ex_mem_to_reg_out, ex_write_from_pc_out;
    wire [1:0]  forward_a, forward_b;
    wire [31:0] mem_forward_data, wb_forward_data;

    ins_ex ex_stage (
        .id_pc_plus_4_in(ex_pc_plus_4_in), .id_pc_in(ex_pc_in), .id_read_data1_in(ex_read_data1_in), .id_read_data2_in(ex_read_data2_in),
        .id_immediate_in(ex_immediate_in), .id_rd_addr_in(ex_rd_addr_in),
        .mem_forward_data_in(mem_forward_data), .wb_forward_data_in(wb_forward_data), .forward_a_in(forward_a), .forward_b_in(forward_b),
        .id_mem_read_in(ex_mem_read_in), .id_mem_write_in(ex_mem_write_in), .id_reg_write_in(ex_reg_write_in), .id_mem_to_reg_in(ex_mem_to_reg_in),
        .id_branch_in(ex_branch_in), .id_jump_in(ex_jump_in), .id_alu_src_in(ex_alu_src_in), .id_alu_ctrl_in(ex_alu_ctrl_in), .id_write_from_pc_in(ex_write_from_pc_in),
        .ex_pc_plus_4_out(ex_pc_plus_4_out), .ex_alu_result_out(ex_alu_result_out), .ex_read_data2_out(ex_read_data2_out), .ex_rd_addr_out(ex_rd_addr_out),
        .ex_mem_read_out(ex_mem_read_out), .ex_mem_write_out(ex_mem_write_out), .ex_reg_write_out(ex_reg_write_out), .ex_mem_to_reg_out(ex_mem_to_reg_out),
        .ex_write_from_pc_out(ex_write_from_pc_out), .ex_branch_taken_out(branch_taken), .ex_branch_target_out(branch_target)
    );


    // EX/MA Buffer
    wire [31:0] ma_pc_plus_4_out, ma_alu_result_out, ma_write_data_out;
    wire [4:0]  ma_rd_addr_out;
    wire        ma_mem_read_out, ma_mem_write_out, ma_reg_write_out, ma_mem_to_reg_out, ma_write_from_pc_out;

    ex_ma_buffer ex_ma (
        .clk(clk), .rst(rst), .en(ex_ma_en),
        .ex_pc_plus_4_in(ex_pc_plus_4_out), .ex_alu_result_in(ex_alu_result_out), .ex_read_data2_in(ex_read_data2_out), .ex_rd_addr_in(ex_rd_addr_out),
        .ex_mem_read_in(ex_mem_read_out), .ex_mem_write_in(ex_mem_write_out), .ex_reg_write_in(ex_reg_write_out), .ex_mem_to_reg_in(ex_mem_to_reg_out),
        .ex_branch_in(1'b0), .ex_write_from_pc_in(ex_write_from_pc_out),
        .ma_pc_plus_4_out(ma_pc_plus_4_out), .ma_alu_result_out(ma_alu_result_out), .ma_write_data_out(ma_write_data_out), .ma_rd_addr_out(ma_rd_addr_out),
        .ma_mem_read_out(ma_mem_read_out), .ma_mem_write_out(ma_mem_write_out), .ma_reg_write_out(ma_reg_write_out), .ma_mem_to_reg_out(ma_mem_to_reg_out),
        .ma_write_from_pc_out(ma_write_from_pc_out)
    );

    // --- MEM Stage ---
    wire [31:0] mem_alu_result_to_wb, mem_read_data_to_wb, mem_pc_plus_4_to_wb;
    wire [4:0]  mem_rd_addr_to_wb;
    wire        mem_reg_write_to_wb, mem_mem_to_reg_to_wb, mem_write_from_pc_to_wb;

    ins_mem mem_stage (
        .clk(clk), .rst(rst), .mem_stall_out(mem_stall),
        .alu_result_in(ma_alu_result_out), .rs2_data_in(ma_write_data_out), .rd_addr_in(ma_rd_addr_out), .pc_plus_4_in(ma_pc_plus_4_out),
        .mem_read_in(ma_mem_read_out), .mem_write_in(ma_mem_write_out), .reg_write_in(ma_reg_write_out), .mem_to_reg_in(ma_mem_to_reg_out), .write_from_pc_in(ma_write_from_pc_out),
        .dbus_addr_out(dbus_addr_out), .dbus_write_data_out(dbus_write_data_out), .dbus_read_en_out(dbus_read_en_out), .dbus_write_en_out(dbus_write_en_out),
        .dbus_read_data_in(dbus_read_data_in), .dbus_ack_in(dbus_ack_in),
        .alu_result_out(mem_alu_result_to_wb), .read_data_out(mem_read_data_to_wb), .rd_addr_out(mem_rd_addr_to_wb), .pc_plus_4_out(mem_pc_plus_4_to_wb),
        .reg_write_out(mem_reg_write_to_wb), .mem_to_reg_out(mem_mem_to_reg_to_wb), .write_from_pc_out(mem_write_from_pc_to_wb)
    );

    // MEM/WB Buffer
    wire [31:0] wb_alu_result_in, wb_read_data_in, wb_pc_plus_4_in;
    wire [4:0]  wb_rd_addr_in;
    wire        wb_reg_write_in, wb_mem_to_reg_in, wb_write_from_pc_in;

    mem_wb_buffer mem_wb (
        .clk(clk), .rst(rst), .en(mem_wb_en),
        .mem_alu_result_in(mem_alu_result_to_wb), .mem_read_data_in(mem_read_data_to_wb), .mem_rd_addr_in(mem_rd_addr_to_wb), .mem_pc_plus_4_in(mem_pc_plus_4_to_wb),
        .mem_reg_write_in(mem_reg_write_to_wb), .mem_to_reg_in(mem_mem_to_reg_to_wb), .mem_write_from_pc_in(mem_write_from_pc_to_wb),
        .wb_alu_result_out(wb_alu_result_in), .wb_read_data_out(wb_read_data_in), .wb_rd_addr_out(wb_rd_addr_in), .wb_pc_plus_4_out(wb_pc_plus_4_in),
        .wb_reg_write_out(wb_reg_write_in), .wb_mem_to_reg_out(wb_mem_to_reg_in), .wb_write_from_pc_out(wb_write_from_pc_in)
    );

    // --- WB Stage ---
    ins_wb wb_stage (
        .clk(clk), .rst(rst),
        .alu_result_in(wb_alu_result_in), .read_data_in(wb_read_data_in), .pc_plus_4_in(wb_pc_plus_4_in), .rd_addr_in(wb_rd_addr_in),
        .reg_write_in(wb_reg_write_in), .mem_to_reg_in(wb_mem_to_reg_in), .write_from_pc_in(wb_write_from_pc_in),
        .wb_write_data_out(wb_write_data_feedback), .wb_rd_addr_out(wb_rd_feedback), .wb_reg_write_en_out(wb_reg_write_feedback)
    );

    // --- Forwarding ---
    assign mem_forward_data = ma_alu_result_out;
    assign wb_forward_data  = wb_write_data_feedback;

    forwarding_unit fwd_unit (
        .ex_rs1_addr(ex_rs1_addr_in), .ex_rs2_addr(ex_rs2_addr_in), .mem_rd_addr(ma_rd_addr_out), .mem_reg_write(ma_reg_write_out),
        .wb_rd_addr(wb_rd_feedback), .wb_reg_write(wb_reg_write_feedback),
        .forward_a(forward_a), .forward_b(forward_b)
    );

    always @(posedge clk) begin
        if (!rst && pipe_en_all) begin
            $display("T=%0t | PC=0x%h | Instr=0x%h", $time, id_pc_in, id_instruction_in);
        end
    end

endmodule
