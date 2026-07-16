# CPU Design — Omni-RISC APU

## Scope (Path A — in-order 5-stage)

RV32IM, single-issue, in-order, 5 stages. This is the resume-defensible core: hazards done properly, M-extension real (not stubbed), CSRs + traps enough to boot bare-metal firmware and pass compliance.

**Not in scope (deferred, do NOT add mid-build):** OoO, ROB, register renaming, dual-issue, branch prediction beyond static-not-taken, out-of-order LSU, superscalar issue. If any of these becomes tempting mid-July, the answer is no.

## Stages

```
IF -> ID -> EX -> MEM -> WB
```

| Stage | Files | What happens |
|-------|-------|--------------|
| IF    | `fetch_stage.v`, `pc_gen.v`                          | PC gen (PC+4 / branch target), I-cache/BRAM read, IF/ID pipeline register |
| ID    | `decode_stage.v`, `decoder.v`, `imm_gen.v`, `regfile.v` | Decode opcode/funct3/funct7, immediate extraction, regfile read, ID/EX register |
| EX    | `exec_stage.v`, `alu.v`, `branch_unit.v`, `multiplier.v`, `divider.v`, `forwarding_net.v` | ALU op, branch resolution, mul/div dispatch, forward from EX/MEM and MEM/WB, EX/MEM register |
| MEM   | `mem_stage.v`, `lsu.v`                              | Load/store to D-cache/BRAM, sign/zero extension, MEM/WB register |
| WB    | `wb_stage.v`                                        | Regfile writeback (single write port, one instr/cycle) |

Control plane: `pipeline_ctrl.v` (stage-valid + squash), `hazard_unit.v` (load-use stall, branch flush), `csr_file.v` (CSR read/write), `trap_unit.v` (exception + mret).

## Hazards

Three types, all handled in-stage — no speculation.

**RAW data hazard (register).** Handled by `forwarding_net.v`. Forwarding paths:
- EX/MEM.rd -> EX.rs1/rs2 (ALU->ALU, 1-cycle back-to-back)
- MEM/WB.rd -> EX.rs1/rs2 (load or old ALU result, 2-cycle gap)

**Load-use hazard.** Cannot forward from MEM output to EX input in the same cycle. `hazard_unit.v` detects `EX.is_load && (EX.rd == ID.rs1 || EX.rd == ID.rs2)` and stalls IF/ID for 1 cycle (bubble injected into EX).

**Control hazard (branch/jump).** Static prediction: not-taken. Branch resolved in EX. On mis-predict, flush IF and ID (2 bubbles). Total branch penalty = 2 cycles. `hazard_unit.v` drives the flush.

## M-extension

`multiplier.v` — 32x32 -> 64, retire in 3-4 cycles (pipelined, no stall needed if issue rate is 1). `divider.v` — iterative, 32 cycles, stalls EX until done. Both integrate as ALU sub-ops selected by decoder.

## CSRs & traps

Minimum viable set for compliance + firmware boot:
- `mstatus`, `mtvec`, `mepc`, `mcause`, `mtval`, `mie`, `mip`
- `mcycle`, `minstret`, `mvendorid` (RO)

Traps: illegal instruction, ecall, misaligned load/store. `trap_unit.v` redirects PC to `mtvec`, saves PC to `mepc`. `mret` returns.

## Memory interface

For now: direct BRAM ports (single-cycle). Cache modules (`hardware/rtl/cpu/cache/l1_icache.v`, `l1_dcache.v`) are stubbed and integrated later — do not block the pipeline bring-up on them.

## Bring-up order (this is the actual build plan)

1. `alu.v` + `tb_alu` — every op, every edge case
2. `regfile.v` + `tb_regfile` — x0 hardwired to zero, two read ports one write port
3. `decoder.v` + `imm_gen.v` + `tb_decoder` — every RV32I opcode
4. `pc_gen.v` + `fetch_stage.v` — sequential fetch from instr BRAM
5. Wire IF->ID->EX->MEM->WB with NO hazards (assume `nop` between every real instr) — `cpu_top.v` boots, executes 3 adds
6. Add `forwarding_net.v` — back-to-back ALU ops work
7. Add `hazard_unit.v` — load-use stall, branch flush
8. `multiplier.v`, `divider.v` — M-extension
9. `csr_file.v` + `trap_unit.v` — ecall works
10. Run riscv-arch-test rv32i, then rv32im
