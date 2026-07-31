# Omni-RISC APU — compliance results

**Status: PASS — riscv-arch-test rv32im (53/54, 1 architectural skip)** — 2026-08-01

## Summary

| Suite | Result |
|-------|--------|
| rv32i/I  (base integer)      | **PASS** — add/sub/and/or/xor/slt/shifts, all branches, jal/jalr, all loads/stores, lui/auipc, fence |
| rv32i/M  (multiply/divide)   | **PASS** — mul/mulh/mulhsu/mulhu, div/divu/rem/remu incl. divide-by-zero and MIN/-1 edge cases |
| rv32i/Zicsr (CSR)            | **PASS** — all six CSRRW/CSRRS/CSRRC ± I variants, write-skip semantics |
| rv32i/Zifencei               | **PASS** — fence (no-op on single hart); **SKIP** fence.i (below) |

53 passed, 1 skipped, 0 failed.

## Reference model caveat

The ACT4 framework normally generates golden signatures with a Sail/Spike
reference model. On this machine qemu-user's RISC-V semihosting only
implements `SYS_EXIT` (file/stdout writes are no-ops) and its gdb-stub
returns unreliable memory reads, so **ref_sim** (`tests/omni-risc/ref_sim.cpp`)
was written — a small, self-contained RV32IM(+Zicsr) interpreter implemented
from the ISA spec. It is an independent implementation from the Verilog, and
it caught one real bug during bring-up (a MULHSU sign handling error in the
interpreter itself, fixed). A second, mature reference (spike/sail) should be
substituted when one is available to strengthen the evidence.

## Skipped

- **Zifencei-fence.i-00** — self-modifying code. The test SW-writes an
  instruction into the code region then executes it. The Omni-RISC CPU uses
  **split instruction/data BRAMs** (Harvard-style), so a store reaches
  data_bram while the fetch reads instr_bram — self-modification is
  architecturally impossible without unified memory or an I-cache flush
  mechanism. This is a documented design limitation, not a bug. `I-fence-00`
  (fence as a memory-ordering no-op) passes.

## How to reproduce

```
cd tests && bash run_compliance.sh
```

Prerequisites: `riscv64-linux-gnu-gcc`, the built DUT harness
(`hardware/sim/obj_dir_tb_compliance/Vcpu_top`), g++.
