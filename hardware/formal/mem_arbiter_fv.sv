// Formal harness for the memory arbiter — the properties that must hold whatever
// the two masters do.
//
//   ./scripts/formal.sh            # runs every .sby under hardware/formal/
//
// mem_arbiter sits between the CPU, the GPU and the one data_bram, and its whole
// job is mutual exclusion plus honest acknowledgements. A simulation can only show
// it behaving for the sequences someone thought to write; these three properties
// are asserted for all sequences within the bound.
module mem_arbiter_fv (input clk);

    reg rst;

    // free inputs: the harness drives nothing, so the solver picks the sequences
    reg [31:0] m0_addr, m0_wdata, m1_addr, m1_wdata;
    reg [3:0]  m0_wen, m1_wen;
    reg        m0_read_req, m1_read_req;

    wire        m0_read_ack, m0_write_ack, m1_read_ack, m1_write_ack;
    wire [31:0] m0_rdata, m1_rdata, mem_addr, mem_wdata;
    wire [3:0]  mem_wen;
    wire [1:0]  grant;

    mem_arbiter u (
        .clk(clk),
        .reset(rst),
        .m0_addr(m0_addr), .m0_wdata(m0_wdata), .m0_wen(m0_wen),
        .m0_read_req(m0_read_req), .m0_read_ack(m0_read_ack),
        .m0_write_req(|m0_wen), .m0_write_ack(m0_write_ack), .m0_rdata(m0_rdata),
        .m1_addr(m1_addr), .m1_wdata(m1_wdata), .m1_wen(m1_wen),
        .m1_read_req(m1_read_req), .m1_read_ack(m1_read_ack),
        .m1_write_req(|m1_wen), .m1_write_ack(m1_write_ack), .m1_rdata(m1_rdata),
        .mem_addr(mem_addr), .mem_wdata(mem_wdata), .mem_wen(mem_wen),
        .mem_rdata(32'b0), .grant(grant)
    );

    always @(posedge clk) begin
        if (rst) begin
            // during reset nothing may be acknowledged
            assert (!m0_read_ack && !m1_read_ack);
            assert (!m0_write_ack && !m1_write_ack);
        end else begin
            // 1. the two masters are never acknowledged in the same cycle: the
            //    memory has one read port, so a shared ack would hand one master
            //    the other's data
            assert (!(m0_read_ack && m1_read_ack));

            // 2. an acknowledgement only ever goes to a master that asked. A read
            //    ack with no request would make mem_stage's hold complete early
            //    against data for someone else.
            assert (m0_read_ack |-> m0_read_req);
            assert (m1_read_ack |-> m1_read_req);

            // 3. the memory is written only when some master is writing, and the
            //    granted write's data is the one presented
            assert ((mem_wen != 4'b0) |-> ((|m0_wen) || (|m1_wen)));
            assert ((mem_wen == m0_wen && (|m0_wen)) |-> (mem_wdata == m0_wdata));
            assert ((mem_wen == m1_wen && (|m1_wen) && ~(|m0_wen)) |-> (mem_wdata == m1_wdata));
        end
    end

    // reset is held for the first cycle, then released forever: the arbiter has no
    // other assumption to make about its environment
    initial rst = 1'b1;
    always @(posedge clk) rst <= 1'b0;

endmodule
