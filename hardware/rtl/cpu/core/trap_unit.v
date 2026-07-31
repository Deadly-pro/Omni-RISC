// Omni-RISC APU — CPU: trap_unit (combinational)
//
// Detects synchronous exceptions and MRET in the EX stage and steers the
// fetch redirect + CSR write port. Mirrors the branch redirect path: when
// an exception fires, the trapping instruction is in EX, so mepc captures
// id_ex_pc and everything younger is flushed by the same mechanism as a
// taken branch (fetch redirect + decode bubble).
//
// Priority: reset > trap/mret > branch redirect (enforced in pc_gen).
// Exceptions and MRET are mutually exclusive (one instruction per slot).
module trap_unit (
    input         id_ex_is_ecall,
    input         id_ex_is_ebreak,
    input         id_ex_illegal,    // decoder: unknown opcode / unknown SYSTEM
    input         id_ex_is_csr,     // gates csr_illegal (CSR-access check)
    input         csr_illegal,      // from csr_file: illegal CSR read/write
    input         id_ex_is_mret,
    input  [31:0] id_ex_pc,

    input  [31:0] mtvec,            // exception vector (from csr_file)
    input  [31:0] mepc,             // mret target   (from csr_file)

    output        trap_valid,       // exception OR mret → redirect fetch
    output [31:0] trap_target,      // mtvec (trap) or mepc (mret)
    output [31:0] trap_pc,          // → csr_file.mepc on trap_taken
    output [31:0] trap_cause,       // ecall=11, ebreak=3, illegal=2
    output [31:0] trap_tval,        // 0 for these synchronous causes
    output        trap_taken,       // → csr_file (exceptions only)
    output        mret              // → csr_file (mstatus pop)
);

    wire is_exception =
        id_ex_illegal | id_ex_is_ecall | id_ex_is_ebreak | (id_ex_is_csr & csr_illegal);

    wire [4:0] cause_code = id_ex_is_ecall ? 5'd11
                          : id_ex_is_ebreak ? 5'd3
                          : 5'd2;                      // illegal (incl. CSR fault)

    assign trap_taken = is_exception;
    assign mret       = id_ex_is_mret;
    assign trap_valid = trap_taken | mret;
    assign trap_target= id_ex_is_mret ? mepc : mtvec;
    assign trap_pc    = id_ex_pc;
    assign trap_cause = {27'b0, cause_code};
    assign trap_tval  = 32'b0;

endmodule
