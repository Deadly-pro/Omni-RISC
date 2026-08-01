module csr_file (
    input             clk,
    input             reset,

    input      [11:0] csr_addr,
    input      [2:0]  csr_funct3,
    input      [31:0] csr_wdata,
    input      [4:0]  csr_uimm,
    input             csr_en,

    output reg [31:0] csr_rdata,
    output reg        csr_illegal,

    input             trap_taken,
    input      [31:0] trap_pc,
    input      [31:0] trap_cause,
    input      [31:0] trap_tval,
    input             mret,
    input             mtip,        // machine timer interrupt pending (from SoC timer)

    output wire [31:0] mtvec_o,
    output reg  [31:0] mepc_o,
    output wire [31:0] mstatus_o,
    output wire [31:0] mie_o,      // for the trap unit (MTIE gating)
    output wire [31:0] mip_o       // for the trap unit (MTIP check)
);

    reg mstatus_mie, mstatus_mpie;
    assign mstatus_o = {19'b0, 2'b11, 3'b0, mstatus_mpie, 3'b0, mstatus_mie, 3'b0};

    reg [31:0] mtvec;
    assign mtvec_o = {mtvec[31:2], 2'b00};

    reg       mcause_int;
    reg [3:0] mcause_code;
    wire [31:0] mcause_val = {mcause_int, 27'b0, mcause_code};

    reg mie_meie, mie_mtie, mie_msie;
    wire [31:0] mie_val = {20'b0, mie_meie, 3'b0, mie_mtie, 3'b0, mie_msie, 3'b0};

    reg mip_meip, mip_msip;
    wire mip_mtip = mtip;   // read-only, set by the SoC timer (CLINT)
    wire [31:0] mip_val = {20'b0, mip_meip, 3'b0, mip_mtip, 3'b0, mip_msip, 3'b0};

    reg [31:0] mscratch;
    reg [31:0] mtval;
    reg [31:0] mcycle;
    reg [31:0] minstret;

    reg invalid_addr;

    always @(*) begin
        invalid_addr = 1'b0;
        case (csr_addr)
            12'h300: csr_rdata = mstatus_o;
            12'h304: csr_rdata = mie_val;
            12'h305: csr_rdata = mtvec;
            12'h340: csr_rdata = mscratch;
            12'h341: csr_rdata = mepc_o;
            12'h342: csr_rdata = mcause_val;
            12'h343: csr_rdata = mtval;
            12'h344: csr_rdata = mip_val;
            12'hB00: csr_rdata = mcycle;
            12'hB02: csr_rdata = minstret;
            12'hF11, 12'hF12, 12'hF13, 12'hF14: csr_rdata = 32'h0000_0000;
            default: begin
                csr_rdata    = 32'h0000_0000;
                invalid_addr = 1'b1;
            end
        endcase
    end

    wire [31:0] src = csr_funct3[2] ? {27'b0, csr_uimm} : csr_wdata;
    wire would_write = (csr_funct3[1:0] == 2'b01) || (csr_uimm != 5'b0);
    wire is_ro = (csr_addr[11:10] == 2'b11);

    always @(*) begin
        csr_illegal = csr_en & (invalid_addr | (would_write & is_ro));
    end

    reg [31:0] next_val;
    always @(*) begin
        case (csr_funct3[1:0])
            2'b01: next_val = src;
            2'b10: next_val = csr_rdata | src;
            2'b11: next_val = csr_rdata & ~src;
            default: next_val = src;
        endcase
    end

    wire write_en = csr_en & ~csr_illegal & would_write;

    assign mie_o = mie_val;
    assign mip_o = mip_val;

    always @(posedge clk) begin
        if (reset) begin
            mstatus_mie  <= 1'b0;
            mstatus_mpie <= 1'b0;
            mtvec        <= 32'b0;
            mcause_int   <= 1'b0;
            mcause_code  <= 4'b0;
            mie_meie     <= 1'b0; mie_mtie <= 1'b0; mie_msie <= 1'b0;
            mip_meip     <= 1'b0; mip_msip <= 1'b0;

            mscratch <= 32'b0;
            mepc_o   <= 32'b0;
            mtval    <= 32'b0;
            mcycle   <= 32'b0;
            minstret <= 32'b0;
        end else begin
            if (trap_taken) begin
                mepc_o       <= trap_pc;
                mtval        <= trap_tval;

                mcause_int   <= trap_cause[31];
                mcause_code  <= trap_cause[3:0];

                mstatus_mpie <= mstatus_mie;
                mstatus_mie  <= 1'b0;
            end
            else if (mret) begin
                mstatus_mie  <= mstatus_mpie;
                mstatus_mpie <= 1'b1;
            end
            else if (write_en) begin
                case (csr_addr)
                    12'h300: begin
                        mstatus_mie  <= next_val[3];
                        mstatus_mpie <= next_val[7];
                    end
                    12'h304: begin
                        mie_msie <= next_val[3];
                        mie_mtie <= next_val[7];
                        mie_meie <= next_val[11];
                    end
                    12'h305: mtvec       <= next_val;
                    12'h340: mscratch   <= next_val;
                    12'h341: mepc_o     <= next_val;
                    12'h342: begin
                        mcause_int  <= next_val[31];
                        mcause_code <= next_val[3:0];
                    end
                    12'h343: mtval <= next_val;
                    12'h344: begin
                        mip_msip <= next_val[3];
                        mip_meip <= next_val[11];
                    end
                    12'hB00: mcycle   <= next_val;
                    12'hB02: minstret <= next_val;
                    default: ;
                endcase
            end
        end
    end

endmodule
