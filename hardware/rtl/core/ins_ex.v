// Execute stage integration for Omni-RISC
`timescale 1ns / 1ps

module ins_ex (
    input  wire [1:0]  id_core_id_in,
    input  wire [31:0] id_pc_plus_4_in,
    input  wire [31:0] id_pc_in,
    input  wire [31:0] id_read_data1_in,
    input  wire [31:0] id_read_data2_in,
    input  wire [31:0] id_immediate_in,
    input  wire [4:0]  id_rs1_addr_in,
    input  wire [4:0]  id_rs2_addr_in,
    input  wire [4:0]  id_rd_addr_in,
    input  wire [31:0] id_instruction_in,
    input  wire        id_mem_read_in,
    input  wire        id_mem_write_in,
    input  wire        id_reg_write_in,
    input  wire        id_mem_to_reg_in,
    input  wire        id_alu_src_in,
    input  wire        id_branch_in,
    input  wire        id_jump_in,
    input  wire [3:0]  id_alu_ctrl_in,
    input  wire        id_write_from_pc_in,
    input  wire [31:0] mem_forward_data_in,
    input  wire [31:0] wb_forward_data_in,
    input  wire [4:0]  mem_rd_addr_in,
    input  wire        mem_reg_write_in,
    input  wire [4:0]  wb_rd_addr_in,
    input  wire        wb_reg_write_in,
    output wire [31:0] ex_pc_plus_4_out,
    output wire [31:0] ex_alu_result_out,
    output wire [31:0] ex_read_data2_out,
    output wire [4:0]  ex_rd_addr_out,
    output wire        ex_mem_read_out,
    output wire        ex_mem_write_out,
    output wire        ex_reg_write_out,
    output wire        ex_mem_to_reg_out,
    output wire        ex_branch_taken_out,
    output wire [31:0] ex_branch_target_out,
    output wire        ex_write_from_pc_out,
    output wire [1:0]  ex_core_id_out,
    output wire [1:0]  forward_a_out,
    output wire [1:0]  forward_b_out
);

    wire [31:0] op_a, op_b_forwarded, op_b;

    forwarding_unit fu (
        .ex_rs1_addr(id_rs1_addr_in),
        .ex_rs2_addr(id_rs2_addr_in),
        .mem_rd_addr(mem_rd_addr_in),
        .mem_reg_write(mem_reg_write_in),
        .wb_rd_addr(wb_rd_addr_in),
        .wb_reg_write(wb_reg_write_in),
        .forward_a(forward_a_out),
        .forward_b(forward_b_out)
    );

    forward_mux mux_a (
        .reg_data_in(id_read_data1_in),
        .mem_forward_data_in(mem_forward_data_in),
        .wb_forward_data_in(wb_forward_data_in),
        .forward_sel_in(forward_a_out),
        .forward_out(op_a)
    );

    forward_mux mux_b (
        .reg_data_in(id_read_data2_in),
        .mem_forward_data_in(mem_forward_data_in),
        .wb_forward_data_in(wb_forward_data_in),
        .forward_sel_in(forward_b_out),
        .forward_out(op_b_forwarded)
    );

    ex_alu_src_mux mux_alu_b (
        .read_data2_in(op_b_forwarded),
        .immediate_in(id_immediate_in),
        .alu_src_sel_in(id_alu_src_in),
        .alu_op_b_out(op_b)
    );

    ex_alu alu (
        .op_a_in(op_a),
        .op_b_in(op_b),
        .alu_ctrl_in(id_alu_ctrl_in),
        .alu_result_out(ex_alu_result_out)
    );

    branch_unit bu (
        .op_a_in(op_a),
        .op_b_in(op_b_forwarded),
        .pc_in(id_pc_in),
        .immediate_in(id_immediate_in),
        .branch_ctrl_in(id_branch_in),
        .jump_ctrl_in(id_jump_in),
        .branch_taken_out(ex_branch_taken_out),
        .branch_target_out(ex_branch_target_out)
    );

    assign ex_pc_plus_4_out     = id_pc_plus_4_in;
    assign ex_read_data2_out    = op_b_forwarded;
    assign ex_rd_addr_out       = id_rd_addr_in;
    assign ex_mem_read_out      = id_mem_read_in;
    assign ex_mem_write_out     = id_mem_write_in;
    assign ex_reg_write_out     = id_reg_write_in;
    assign ex_mem_to_reg_out    = id_mem_to_reg_in;
    assign ex_write_from_pc_out = id_write_from_pc_in;
    assign ex_core_id_out       = id_core_id_in;

endmodule

module forwarding_unit (
    input  wire [4:0] ex_rs1_addr,
    input  wire [4:0] ex_rs2_addr,
    input  wire [4:0] mem_rd_addr,
    input  wire       mem_reg_write,
    input  wire [4:0] wb_rd_addr,
    input  wire       wb_reg_write,
    output reg  [1:0] forward_a,
    output reg  [1:0] forward_b
);
    always @(*) begin
        forward_a = 2'b00;
        forward_b = 2'b00;

        if (mem_reg_write && (mem_rd_addr != 5'b0) && (mem_rd_addr == ex_rs1_addr))
            forward_a = 2'b10;
        else if (wb_reg_write && (wb_rd_addr != 5'b0) && (wb_rd_addr == ex_rs1_addr))
            forward_a = 2'b01;

        if (mem_reg_write && (mem_rd_addr != 5'b0) && (mem_rd_addr == ex_rs2_addr))
            forward_b = 2'b10;
        else if (wb_reg_write && (wb_rd_addr != 5'b0) && (wb_rd_addr == ex_rs2_addr))
            forward_b = 2'b01;
    end
endmodule

module forward_mux (
    input  wire [31:0] reg_data_in,
    input  wire [31:0] mem_forward_data_in,
    input  wire [31:0] wb_forward_data_in,
    input  wire [1:0]  forward_sel_in,
    output reg  [31:0] forward_out
);
    always @(*) begin
        case (forward_sel_in)
            2'b00:   forward_out = reg_data_in;
            2'b01:   forward_out = wb_forward_data_in;
            2'b10:   forward_out = mem_forward_data_in;
            default: forward_out = reg_data_in;
        endcase
    end
endmodule

module id_ex_buffer (
    input  wire        clk,
    input  wire        rst,
    input  wire        en,
    input  wire        clr,
    input  wire [1:0]  id_core_id_in,
    input  wire [31:0] id_pc_plus_4_in,
    input  wire [31:0] id_pc_in[0:3],
    input  wire [31:0] id_read_data1_in,
    input  wire [31:0] id_read_data2_in,
    input  wire [31:0] id_immediate_in,
    input  wire [4:0]  id_rs1_addr_in,
    input  wire [4:0]  id_rs2_addr_in,
    input  wire [4:0]  id_rd_addr_in,
    input  wire [31:0] id_instruction_in,
    input  wire        id_mem_read_in,
    input  wire        id_mem_write_in,
    input  wire        id_reg_write_in,
    input  wire        id_mem_to_reg_in,
    input  wire        id_alu_src_in,
    input  wire        id_branch_in,
    input  wire        id_jump_in,
    input  wire [3:0]  id_alu_ctrl_in,
    input  wire        id_write_from_pc_in,
    output reg  [31:0] ex_pc_plus_4_out,
    output reg  [31:0] ex_pc_out[0:3],
    output reg  [31:0] ex_read_data1_out,
    output reg  [31:0] ex_read_data2_out,
    output reg  [31:0] ex_immediate_out,
    output reg  [4:0]  ex_rs1_addr_out,
    output reg  [4:0]  ex_rs2_addr_out,
    output reg  [4:0]  ex_rd_addr_out,
    output reg  [31:0] ex_instruction_out,
    output reg         ex_mem_read_out,
    output reg         ex_mem_write_out,
    output reg         ex_reg_write_out,
    output reg         ex_mem_to_reg_out,
    output reg         ex_alu_src_out,
    output reg         ex_branch_out,
    output reg         ex_jump_out,
    output reg  [3:0]  ex_alu_ctrl_out,
    output reg         ex_write_from_pc_out,
    output reg  [1:0]  ex_core_id_out
);
    always @(posedge clk or posedge rst) begin
        if (rst || clr) begin
            ex_pc_plus_4_out <= 32'b0;
            ex_pc_out[0] <= 32'b0;
            ex_pc_out[1] <= 32'b0;
            ex_pc_out[2] <= 32'b0;
            ex_pc_out[3] <= 32'b0;
            ex_instruction_out <= 32'h00000013;
            ex_mem_read_out <= 0; ex_mem_write_out <= 0; ex_reg_write_out <= 0;
            ex_mem_to_reg_out <= 0; ex_alu_src_out <= 0; ex_branch_out <= 0; ex_jump_out <= 0;
            ex_write_from_pc_out <= 0;
            ex_core_id_out <= 2'b00;
        end else if (en) begin
            ex_pc_plus_4_out <= id_pc_plus_4_in;
            ex_pc_out[0] <= id_pc_in[0];
            ex_pc_out[1] <= id_pc_in[1];
            ex_pc_out[2] <= id_pc_in[2];
            ex_pc_out[3] <= id_pc_in[3];
            ex_read_data1_out <= id_read_data1_in; ex_read_data2_out <= id_read_data2_in;
            ex_immediate_out <= id_immediate_in; ex_rs1_addr_out <= id_rs1_addr_in;
            ex_rs2_addr_out <= id_rs2_addr_in; ex_rd_addr_out <= id_rd_addr_in;
            ex_instruction_out <= id_instruction_in; ex_mem_read_out <= id_mem_read_in;
            ex_mem_write_out <= id_mem_write_in; ex_reg_write_out <= id_reg_write_in;
            ex_mem_to_reg_out <= id_mem_to_reg_in; ex_alu_src_out <= id_alu_src_in;
            ex_branch_out <= id_branch_in; ex_jump_out <= id_jump_in; ex_alu_ctrl_out <= id_alu_ctrl_in;
            ex_write_from_pc_out <= id_write_from_pc_in;
            ex_core_id_out <= id_core_id_in;
        end
    end
endmodule

module branch_unit (
    input  wire [31:0] op_a_in,
    input  wire [31:0] op_b_in,
    input  wire [31:0] pc_in,
    input  wire [31:0] immediate_in,
    input  wire        branch_ctrl_in,
    input  wire        jump_ctrl_in,
    output wire        branch_taken_out,
    output wire [31:0] branch_target_out
);
    wire is_equal = (op_a_in == op_b_in);
    assign branch_taken_out = jump_ctrl_in || (branch_ctrl_in && is_equal);
    assign branch_target_out = pc_in + immediate_in;
endmodule

module ex_alu (
    input  wire [31:0] op_a_in,
    input  wire [31:0] op_b_in,
    input  wire [3:0]  alu_ctrl_in,
    output reg  [31:0] alu_result_out
);
    always @(*) begin
        case (alu_ctrl_in)
            4'b0000: alu_result_out = op_a_in + op_b_in; // ADD
            4'b0001: alu_result_out = op_a_in - op_b_in; // SUB
            4'b0010: alu_result_out = op_a_in & op_b_in; // AND
            4'b0011: alu_result_out = op_a_in | op_b_in; // OR
            4'b0100: alu_result_out = op_a_in ^ op_b_in; // XOR
            4'b0101: alu_result_out = op_b_in;           // LUI (Pass Imm)
            4'b0110: alu_result_out = op_a_in << op_b_in[4:0]; // SLL
            4'b0111: alu_result_out = ($signed(op_a_in) < $signed(op_b_in)) ? 32'b1 : 32'b0; // SLT
            4'b1000: alu_result_out = (op_a_in < op_b_in) ? 32'b1 : 32'b0; // SLTU
            4'b1001: alu_result_out = op_a_in >> op_b_in[4:0];  // SRL
            4'b1010: alu_result_out = $signed(op_a_in) >>> op_b_in[4:0]; // SRA
            default: alu_result_out = 32'b0;
        endcase
    end
endmodule

module ex_alu_src_mux (
    input  wire [31:0] read_data2_in,
    input  wire [31:0] immediate_in,
    input  wire        alu_src_sel_in,
    output reg  [31:0] alu_op_b_out
);
    always @(*) begin
        case (alu_src_sel_in)
            1'b0: alu_op_b_out = read_data2_in;
            1'b1: alu_op_b_out = immediate_in;
            default: alu_op_b_out = read_data2_in;
        endcase
    end
endmodule
