# Deferred cleanup — do before the final push (pre-synthesis / pre-writeup)

## Code quality
- [ ] **localparams for magic numbers** — add named constants in `decoder.v` and `alu.v`:
      `localparam IMM_I=0, IMM_S=1, IMM_B=2, IMM_U=3, IMM_J=4, IMM_NONE=5;`
      `localparam ALU_ADD=0, ALU_SUB=1, ALU_AND=2, ALU_OR=3, ALU_XOR=4, ALU_SLT=5, ALU_SLTU=6, ALU_SLL=7, ALU_SRL=8, ALU_SRA=9, ALU_LUI=10, ALU_AUIPC=11;`
      Use them in every case label/assignment. Rerun tb_alu + tb_decoder after.
- [ ] Delete dead empty `case(funct3)` blocks in decoder Load/Store arms
      (funct3 passes through as an output; the LSU decides byte/half/word)
- [ ] Remove unused `reg [31:0] temp` in alu.v
- [ ] Fix comment typo "invaid" in decoder.v; normalize indentation across arms
- [ ] Trim spec headers before final polish: keep PORTS + GOTCHAS (good docs),
      cut the "TODAY: code + test with..." coaching lines and DONE WHEN sections

## Correctness hardening (do during compliance testing, step 10)
- [ ] Tighten illegal_instr: `default: illegal_instr=1;` in the inner
      funct3/funct7 cases (R-type with bogus funct7 currently decodes as ADD,
      Load with funct3=3 silently passes)
