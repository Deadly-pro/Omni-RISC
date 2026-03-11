module csr_file(
    input wire clk,
    input wire rst,
    input wire csr_addr_in,
    input wire [31:0] csr_write_data_in,
    input wire        csr_write_en_in,
    input wire        csr_read_en_in,
    input wire        csr_read_valid_in,
    output wire [31:0] csr_read_data_out,
    output wire       csr_write_en_out,
    output wire       csr_read_en_out,
    output wire       csr_read_valid_out
);
endmodule
