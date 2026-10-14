// Omni-RISC APU — shared data-memory arbiter
//
// Two masters (the CPU and, from M2 on, the GPU) against one data_bram. Not to be
// confused with pbus_arbiter, which is a PERIPHERAL arbiter (read/ready, 1-cycle
// grant) used by dual_core_top for UART/TIMER/GPIO; peripherals are always ready,
// memory is not, so this one speaks the req/ack handshake that data_bram's
// registered read implies.
//
// Policy: the CPU wins whenever it is asking. The CPU's core memory path is
// latency-critical (one extra wait state is a pipeline stall) and its timing must
// be exactly what it was when data_bram lived inside mem_stage, whereas a GPU
// access that loses arbitration simply retries - the M2 warp stall holds the
// instruction until its request is granted. In the intended usage the CPU is not
// hammering memory while a kernel runs: it launches and then waits on the GPU's
// completion interrupt, so the GPU sees the memory for the whole kernel.
//
// Reads: data_bram's read is REGISTERED, so the response to a request arrives one
// cycle later. `rtag` records who that response belongs to.
// Writes: synchronous, and they must not be deferred - a store completes in the
// cycle it is presented - so a GPU write in a cycle the CPU is also writing is
// simply not taken (`m1_write_ack` stays low) and the GPU retries.
module mem_arbiter (
    input         clk,
    input         reset,

    // master 0: CPU (priority)
    input  [31:0] m0_addr,
    input  [31:0] m0_wdata,
    input  [3:0]  m0_wen,
    input         m0_read_req,
    output        m0_read_ack,
    input         m0_write_req,
    output        m0_write_ack,
    output [31:0] m0_rdata,

    // master 1: GPU
    input  [31:0] m1_addr,
    input  [31:0] m1_wdata,
    input  [3:0]  m1_wen,
    input         m1_read_req,
    output        m1_read_ack,
    input         m1_write_req,
    output        m1_write_ack,
    output [31:0] m1_rdata,

    // memory side
    output [31:0] mem_addr,
    output [31:0] mem_wdata,
    output [3:0]  mem_wen,
    input  [31:0] mem_rdata,

    output [1:0]  grant        // 0 = idle, 1 = CPU, 2 = GPU (debug)
);

    wire m0_write = |m0_wen;
    wire m1_write = |m1_wen;

    wire m0_act = m0_read_req | m0_write;
    wire m1_act = m1_read_req | m1_write;

    // The GPU is presented only when the CPU is silent and no response is still
    // in flight (a response belongs to the request that caused it).
    reg [1:0] rtag;
    wire take_gpu = ~m0_act & m1_act & (rtag == 2'b00);

    assign mem_addr  = m0_act ? m0_addr  : (take_gpu ? m1_addr  : 32'b0);
    assign mem_wdata = m0_act ? m0_wdata : (take_gpu ? m1_wdata : 32'b0);
    assign mem_wen   = m0_act ? m0_wen   : (take_gpu ? m1_wen   : 4'b0);

    always @(posedge clk) begin
        if (reset) rtag <= 2'b00;
        else       rtag <= (m0_act & m0_read_req) ? 2'b01 :
                           ((take_gpu & m1_read_req) ? 2'b10 : 2'b00);
    end

    assign m0_read_ack  = (rtag == 2'b01);
    assign m1_read_ack  = (rtag == 2'b10);
    assign m0_write_ack = m0_act & m0_write;
    assign m1_write_ack = take_gpu & m1_write;

    // the registered read's data goes to whoever asked; the CPU's cacheless path
    // is self-timed off its own hold toggle and ignores the ack
    assign m0_rdata = mem_rdata;
    assign m1_rdata = mem_rdata;

    assign grant = m0_act ? 2'b01 : (take_gpu ? 2'b10 : 2'b00);

endmodule
