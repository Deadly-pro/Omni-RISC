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

    // interrupt enables / pending (from csr_file)
    input         mstatus_mie,       // global interrupt enable (mstatus.MIE)
    input         mie_mtie,          // machine timer interrupt enable (mie.MTIE)
    input         mip_mtip,          // machine timer interrupt pending (mip.MTIP)
    input         mie_msie,          // machine software interrupt enable (mie.MSIE)
    input         mip_msip,          // machine software interrupt pending (mip.MSIP)

    // pipeline-stability gate (from exec_stage): an interrupt must not be
    // taken while a load is in MEM, because the instruction in EX may be
    // consuming its forwarded value. Flushing now would make the resume
    // re-execute that instruction against a stale register file (FreeRTOS
    // queue ORDER FAIL). ex_mem_mem_read is a register, so this gate cannot
    // combinational-loop with the trap->flush path (a load-use hazard in EX
    // is fine: mepc = the load, and the resume re-executes it from memory).
    input         ex_mem_mem_read,

    // redirect-drain info (from exec_stage): a branch/jump redirected recently
    // and wrong-path instructions are draining. An interrupt taken now must use
    // the redirect target as mepc, not the wrong-path id_ex_pc.
    input         redirect_pending,
    input  [31:0] redirect_target,

    output        trap_valid,       // exception OR mret → redirect fetch
    output [31:0] trap_target,      // mtvec (trap) or mepc (mret)
    output [31:0] trap_pc,          // → csr_file.mepc on trap_taken
    output [31:0] trap_cause,       // ecall=11, ebreak=3, illegal=2, timer-int=0x80000007
    output [31:0] trap_tval,        // 0 for these synchronous causes
    output        trap_taken,       // → csr_file (exceptions only)
    output        mret              // → csr_file (mstatus pop)
);

    wire is_exception =
        id_ex_illegal | id_ex_is_ecall | id_ex_is_ebreak | (id_ex_is_csr & csr_illegal);

    // Take the interrupt only when id_ex_pc is a real instruction (not a
    // flush bubble) or the pipeline is draining a redirect (wrong-path is in
    // EX, so mepc must come from the redirect target instead). Also require a
    // stable register file: no load in MEM (whose forwarded value a younger EX
    // instruction may consume). Otherwise the trap flush would make the resume
    // re-execute that instruction against a stale register value.
    // Spec priority MEI > MSI > MTI; we implement MSI over MTI.
    wire int_window = ((id_ex_pc != 32'b0) | redirect_pending) & ~ex_mem_mem_read;
    wire soft_int  = mstatus_mie & mie_msie & mip_msip & int_window;
    wire timer_int = mstatus_mie & mie_mtie & mip_mtip & int_window & ~soft_int;

    wire [4:0] cause_code = id_ex_is_ecall ? 5'd11
                          : id_ex_is_ebreak ? 5'd3
                          : 5'd2;                      // illegal (incl. CSR fault)

    assign trap_taken = is_exception | timer_int | soft_int;
    assign mret       = id_ex_is_mret;
    assign trap_valid = trap_taken | mret;
    assign trap_target= id_ex_is_mret ? mepc : mtvec;
    assign trap_pc    = ((timer_int | soft_int) & redirect_pending) ? redirect_target : id_ex_pc;
    assign trap_cause = soft_int  ? 32'h8000_0003 :
                        timer_int ? 32'h8000_0007 : {27'b0, cause_code};
    assign trap_tval  = 32'b0;

endmodule
