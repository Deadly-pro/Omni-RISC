# Code-quality backlog (optional polish — core is verified)

Items below are cosmetic/hardening improvements that do NOT affect current
results (compliance 53/54, tb_cpu_top 83/83, full regression green). Do them
only when the schedule allows; re-run the affected TB after each.

## Done (2026-08-20)
- [x] Fix comment typo "invaid" → "invalid" in `decoder.v`
- [x] Remove unused `reg [31:0] temp` in `alu.v`

## Deferred — cosmetic
- [ ] **localparams for magic numbers** in `decoder.v` and `alu.v`:
      `localparam IMM_I=0, IMM_S=1, IMM_B=2, IMM_U=3, IMM_J=4, IMM_NONE=5;`
      `localparam ALU_ADD=0, ALU_SUB=1, ALU_AND=2, ALU_OR=3, ALU_XOR=4, ALU_SLT=5, ALU_SLTU=6, ALU_SLL=7, ALU_SRL=8, ALU_SRA=9, ALU_LUI=10, ALU_AUIPC=11;`
      Use them in every case label/assignment. Rerun tb_alu + tb_decoder.
- [ ] Delete dead empty `case(funct3)` blocks in decoder Load/Store arms
      (funct3 passes through as an output; the LSU decides byte/half/word)
- [ ] Normalize indentation in `decoder.v` arms
- [ ] Trim spec headers: keep PORTS + GOTCHAS (good docs), cut the "TODAY:
      code + test with..." coaching lines and DONE WHEN sections

## Deferred — correctness hardening (needs full regression + compliance)
- [ ] Tighten illegal_instr: `default: illegal_instr=1;` in the inner
      funct3/funct7 cases (R-type with bogus funct7 currently decodes as ADD,
      Load with funct3=3 silently passes). `tb_decoder` has the `exp_illegal`
      check wired but no test vector currently exercises `true` — add vectors
      first, then gate the change on compliance staying 53/54.
