# Omni-RISC APU — 10-Day Revision Plan (Path A)

> Path A: in-order 5-stage RV32IM. OoO / ROB / RAT / GShare are out of scope.
> Every exercise maps to a specific file in this project.

---

## Master Resource Table

### Books (You Should Own These)

| # | Book | Author | What You Need From It |
|---|------|--------|----------------------|
| B1 | Computer Organization and Design: RISC-V Edition (2nd ed) | Patterson & Hennessy | Ch 4: pipeline, hazards, forwarding, in-order control. Ch 5: caches. Green Card at back. |
| B2 | Programming Massively Parallel Processors (4th ed) | Kirk, Hwu, Hajj | Ch 1–6: GPU architecture, SIMT, memory hierarchy, coalescing. **The GPU textbook.** |
| B3 | FPGA Prototyping by Verilog Examples | Pong P. Chu | Verilog coding patterns for FPGA, BRAM inference, UART, SPI |
| B4 | Computer Architecture: A Quantitative Approach (6th ed) | H&P | Ch 4 only — GPUs vs vector processors, one dense chapter. Skip Ch 3 (OoO — not this scope). |

### Online Courses & Lectures

| # | Resource | Link | Topics |
|---|----------|------|--------|
| L1 | **Onur Mutlu — Digital Design & Computer Architecture (ETH)** | https://www.youtube.com/@OnurMutluLectures | Pipelining, memory hierarchy, GPU architecture. Skip the OoO lectures for now. |
| L2 | **Berkeley CS61C** | https://inst.eecs.berkeley.edu/~cs61c/ | Lectures 20–23: pipelining + hazards. Tightest free coverage of Path A material. |
| L3 | **NVIDIA GTC — "How GPU Computing Works" (Stephen Jones)** | YouTube search | 45-min visual walkthrough of SM, warp execution, memory hierarchy. Watch once before Day 8. |

### Interactive Practice

| # | Resource | Link | Topics |
|---|----------|------|--------|
| P1 | **HDLBits** | https://hdlbits.01xz.net/wiki/Problem_sets | Verilog exercises with instant feedback. Skip if 4-core project is fresh. |
| P2 | **ChipDev** | https://chipdev.io/question-list | Verilog & VLSI interview questions. Harder than HDLBits. |

### RISC-V References

| # | Resource | Link | Topics |
|---|----------|------|--------|
| R1 | **RISC-V ISA Spec (Unprivileged) v20191213** | https://riscv.org/specifications/ | Ch 2 (RV32I), Ch 7 (RV32M). |
| R2 | **RISC-V Privileged Spec** | https://riscv.org/specifications/ | Ch 3 only (M-mode): mstatus, mtvec, mepc, mcause, mie/mip. Skip S/U mode. |
| R3 | **RISC-V Green Card** | https://inst.eecs.berkeley.edu/~cs61c/resources/RISCV_Reference_Card.pdf | Opcode / funct3 / funct7 quick reference. Print it. |
| R4 | **rvcodecjs** | https://luplab.gitlab.io/rvcodecjs/ | Verify your hand-encoding exercises. |

### GPU Architecture (for August + interview signal)

| # | Resource | Link | Topics |
|---|----------|------|--------|
| G1 | **PMPP 4th ed (B2)** — Ch 1–6 core, 7–10 memory | book | THE undergrad GPU architecture text |
| G2 | **NVIDIA CUDA C++ Programming Guide — Ch 5 + Ch 4** | https://docs.nvidia.com/cuda/cuda-c-programming-guide/ | Warp/SIMT/coalescing/occupancy from the vendor |
| G3 | **NVIDIA whitepapers — Volta then Ampere** | https://www.nvidia.com/en-us/data-center/resources/ | Exactly what interviewers assume you've read |
| G4 | **Volkov 2016 — "Understanding Latency Hiding on GPUs"** | https://www2.eecs.berkeley.edu/Pubs/TechRpts/2016/EECS-2016-143.html | Reframes occupancy. Read after PMPP Ch 5. |
| G5 | **Simty (INRIA)** | https://team.inria.fr/pacap/software/simty/ | Synthesizable SIMT core in Verilog — reference RTL |
| G6 | **Vortex GPGPU** | https://github.com/vortexgpgpu/vortex | Full open-source RISC-V GPGPU in SystemVerilog. Study structure, don't copy. |
| G7 | **Onur Mutlu GPU lectures** (L1) | YouTube | SIMT vs SIMD, warp scheduling, divergence |

### Embedded C / Bare-Metal

| # | Resource | Link | Topics |
|---|----------|------|--------|
| E1 | **"RISC-V from scratch"** | Search "twilco RISC-V from scratch" | Startup asm, linker script, bare-metal boot |
| E2 | **Five EmbedDev** | https://five-embeddev.com | RISC-V bare-metal C, interrupts, CSR access |

### Verilator & Simulation

| # | Resource | Link | Topics |
|---|----------|------|--------|
| S1 | **Verilator User Guide** | https://verilator.org/guide/latest/ | C++ testbench API, tracing, --trace flag |
| S2 | **ZipCPU Verilator Tutorial** | Search "ZipCPU Verilator tutorial" | Practical testbench examples |

### Compliance / Verification

| # | Resource | Link | Topics |
|---|----------|------|--------|
| V1 | **riscv-arch-test** | https://github.com/riscv-non-isa/riscv-arch-test | The suite `tests/run_compliance.sh` runs |
| V2 | **riscv-formal** | https://github.com/YosysHQ/riscv-formal | Formal verification of RV32I cores. Optional; strong interview signal. |

---

## Day-by-Day Plan

---

### Day 1 — Verilog: Combinational Logic Refresh

**Time**: ~4 hours (skip to 1 hour and only redo the ALU exercise if 4-core project is fresh)

**Goal**: Recover Verilog muscle memory for combinational circuits.

#### HDLBits Exercises

Do https://hdlbits.01xz.net/wiki/Problem_sets sections:

| Category | Problems | Est. Time | Why |
|----------|----------|-----------|-----|
| Getting Started → Step One | 1 problem | 2 min | Remember `module`, `endmodule`, `assign` |
| Verilog Language → Basics | Wire, Wire4, Notgate, Andgate, Norgate, Xnorgate, Wire_decl | 15 min | Syntax recovery |
| Verilog Language → Vectors | Vector0–5, Vectorgates, Gates4 | 20 min | Bit slicing, concatenation |
| Verilog Language → Modules: Hierarchy | Module, Module_pos, Module_name, Module_shift, Module_shift8, Module_cseladd, Module_addsub | 30 min | You'll instantiate ~15 submodules in `cpu_top.v` |
| Circuits → Combinational Logic → Basic Gates | All | 15 min | Warm-up |
| Circuits → Combinational Logic → Multiplexers | Mux2to1, Mux2to1v, Mux9to1v, Mux256to1, Mux256to1v | 20 min | ALU / forwarding mux / PC mux are all muxes |
| Circuits → Combinational Logic → Arithmetic | Hadd, Fadd, Adder, Signed overflow | 20 min | ALU internals |
| Circuits → Combinational Logic → Karnaugh Maps | All | 20 min | Decode logic |

#### Self-Test Exercise (30 min)

Write from scratch, no references:

```
A parameterized N-bit ALU that supports:
  - ADD, SUB, AND, OR, XOR, SLT, SLTU, SLL, SRL, SRA
  - Input:  [N-1:0] a, b; [3:0] op
  - Output: [N-1:0] result; zero_flag
Use a case statement on op.
```

**Maps to**: `hardware/rtl/cpu/core/alu.v`
**Test with**: `hardware/sim/cpu/tb_alu.cpp`

#### Checkpoint

You can write a `case`-based mux, an `assign`-based mux, and a parameterized module header from memory. You know the difference between `wire`, `reg`, `logic`, `assign`, `always @(*)`, and `always @(posedge clk)`.

---

### Day 2 — Verilog: Sequential Logic + FSMs

**Time**: ~4–5 hours

**Goal**: Clocked logic and state machines. Almost every module in the CPU is sequential.

#### HDLBits Exercises

| Category | Problems | Est. Time | Why |
|----------|----------|-----------|-----|
| Sequential → Latches and Flip-Flops | Dff, Dff8, Dff8r, Dff8p, Dff8ar, Dff16e | 20 min | Every pipeline register is a DFF |
| Sequential → Counters | Count15, Count10, Count1to10, Countslow, rest | 25 min | Cycle counter, timer, cache LRU |
| Sequential → Shift Registers | Shift4, Rotate100, Shift18, Lfsr5, rest | 25 min | Trace buffers, serial IO |
| Sequential → More Circuits | Rule90, Rule110, Conway's | 30 min | Complex sequential patterns |
| Sequential → FSMs | Fsm1, Fsm1s, Fsm2, Fsm2s, Fsm3comb, Fsm3onehot, Fsm3, Lemmings1–4, Fsm_serial, Fsm_serialdata, Fsm_ps2 | 60 min | **Critical** — CPU control, UART, divider, hazard unit are FSMs |
| Building Larger Circuits | Counter with period 1000, sequence recognizer, timer | 30 min | Integration |

#### Self-Test Exercise: UART Transmitter FSM (45 min)

```
module uart_tx (
    input  clk, reset,
    input  [7:0] tx_data,
    input  tx_valid,         // pulse to start transmission
    output reg tx_out,       // serial output line
    output     tx_busy       // high while transmitting
);
// States: IDLE -> START_BIT -> DATA[0..7] -> STOP_BIT -> IDLE
// Baud rate counter (parameter CLKS_PER_BIT)
// 2-block FSM style
```

**Maps to**: `hardware/rtl/peripherals/uart.v`

#### Key Patterns to Memorize

**2-block FSM style** (use this everywhere):
```verilog
// Block 1: state register
always @(posedge clk or posedge reset) begin
    if (reset) state <= IDLE;
    else       state <= next_state;
end

// Block 2: next-state + output logic
always @(*) begin
    next_state = state;        // default: hold
    case (state)
        IDLE:    if (start) next_state = RUNNING;
        RUNNING: if (done)  next_state = IDLE;
    endcase
end
```

**Pipeline register pattern** (used in every pipeline stage):
```verilog
always @(posedge clk) begin
    if (reset || flush) begin
        ex_alu_result <= 0;
        ex_rd         <= 0;
    end else if (!stall) begin
        ex_alu_result <= id_alu_result;
        ex_rd         <= id_rd;
    end
    // stall: implicit hold
end
```

#### Checkpoint

You can write a 5+ state Moore FSM in 2-block style from memory. You understand synchronous vs asynchronous reset and the stall/flush pipeline register pattern.

---

### Day 3 — Verilog: Memories, Parameters & Testbenches

**Time**: ~4 hours

**Goal**: BRAM inference, parameterized modules, `$readmemh`, Verilator C++ testbenches.

#### Study: BRAM Inference Patterns

Xilinx FPGAs infer BRAM vs distributed RAM from the coding style:

```verilog
// BRAM (synchronous read) — instr/data memory, scratchpad
(* ram_style = "block" *) reg [31:0] mem [0:1023];
always @(posedge clk) begin
    if (wen) mem[waddr] <= wdata;
    rdata <= mem[raddr];          // sync read -> BRAM
end

// Distributed RAM (async read) — regfile
reg [31:0] mem [0:31];
always @(posedge clk) begin
    if (wen) mem[waddr] <= wdata;
end
assign rdata = mem[raddr];        // async read -> LUT RAM
```

Why: regfile must be async-read so decode can consume rs1/rs2 the same cycle it dispatches. Instr/data memory can be sync-read (matches BRAM natively).

#### Self-Test Exercises

**Exercise 1: Parameterized register file (20 min)**
```verilog
module regfile #(
    parameter DEPTH = 32,
    parameter WIDTH = 32,
    parameter ADDR_W = $clog2(DEPTH)
)(
    input  clk, reset,
    input  [ADDR_W-1:0] rs1_addr, rs2_addr, rd_addr,
    input  [WIDTH-1:0]  rd_data,
    input  rd_write_en,
    output [WIDTH-1:0]  rs1_data, rs2_data
);
// 2 async read ports, 1 sync write port. x0 hardwired to zero.
endmodule
```
**Maps to**: `hardware/rtl/cpu/core/regfile.v`

**Exercise 2: Simple direct-mapped cache (30 min)**
```
- 1KB, direct-mapped, 16-byte lines
- Ports: addr, rdata, wdata, ren, wen, hit, miss
- Tag + valid bit array; on hit return data, on miss assert miss signal
```
**Maps to**: `hardware/rtl/cpu/cache/l1_icache.v`, `l1_dcache.v`

#### Verilator Testbench Refresher

Open `hardware/sim/cpu/tb_alu.cpp` and `hardware/sim/run_sim.sh`. Understand the flow: `run_sim.sh cpu/tb_<name>` auto-finds `<name>.v` under `hardware/rtl/` and builds with `--cc --exe --trace`.

**Key C++ pattern**:
```cpp
#include "Vdut_name.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vdut_name* dut = new Vdut_name;

    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("waves/trace.vcd");

    for (int cycle = 0; cycle < 1000; cycle++) {
        dut->clk = 0; dut->eval(); tfp->dump(cycle*2);
        dut->clk = 1; dut->eval(); tfp->dump(cycle*2+1);
        // check outputs here
    }

    tfp->close();
    delete dut;
    return 0;
}
```

#### Checkpoint

You know BRAM vs distributed RAM inference. You can write a Verilator C++ testbench. You know `$readmemh` and `$clog2`.

---

### Day 4 — RISC-V ISA: Instruction Encoding

**Time**: ~4 hours

**Goal**: Hand-encode and hand-decode any RV32IM instruction cold. This becomes your `decoder.v`.

#### Read

- RISC-V Green Card (R3) — print it, keep on desk
- RISC-V Unprivileged Spec (R1) — Ch 2 (RV32I), Ch 7 (RV32M)

#### Exercise 1: Encode These Instructions (pen & paper)

Full 32-bit binary + hex. Verify each with rvcodecjs (R4).

| # | Instruction | Type | Hex |
|---|---|---|---|
| 1 | `add  x3, x1, x2` | R | `0x002081B3` (example) |
| 2 | `sub  x5, x3, x4` | R | |
| 3 | `and  x6, x7, x8` | R | |
| 4 | `mul  x3, x1, x2` | R | |
| 5 | `div  x4, x5, x6` | R | |
| 6 | `addi x1, x0, 42` | I | |
| 7 | `slli x3, x1, 5` | I | |
| 8 | `lw   x7, 8(x2)` | I | |
| 9 | `lb   x3, -1(x5)` | I | |
| 10 | `sw   x7, 12(x2)` | S | |
| 11 | `sh   x3, 6(x4)` | S | |
| 12 | `beq  x1, x2, +16` | B | |
| 13 | `bne  x3, x0, -8` | B | |
| 14 | `blt  x1, x2, +32` | B | |
| 15 | `lui  x5, 0xDEADB` | U | |
| 16 | `auipc x6, 0x10` | U | |
| 17 | `jal  x1, +100` | J | |
| 18 | `jalr x1, x5, 0` | I | |
| 19 | `csrrw x3, mstatus, x1` | I (CSR) | |
| 20 | `ecall` | I | |

#### Exercise 2: Decode These Hex Values

| Hex | Assembly |
|---|---|
| `0x002081B3` | `add x3, x1, x2` (example) |
| `0x00A12023` | |
| `0xFE5290E3` | |
| `0x00500113` | |
| `0xDEADB2B7` | |
| `0x0000006F` | |
| `0x02208233` | |

#### Key Insight for Your Decoder

RISC-V puts every field in the SAME bit position, always:
```verilog
wire [6:0] opcode = instruction[6:0];
wire [2:0] funct3 = instruction[14:12];
wire [6:0] funct7 = instruction[31:25];
wire [4:0] rd     = instruction[11:7];
wire [4:0] rs1    = instruction[19:15];
wire [4:0] rs2    = instruction[24:20];
```
Only the immediate assembly changes across formats.

**Maps to**: `hardware/rtl/cpu/core/decoder.v`, `imm_gen.v`

#### Checkpoint

You can decode `0xFE5290E3` in under 2 minutes. You know all 6 instruction formats. You memorized the opcodes: R (`0110011`), I-ALU (`0010011`), Load (`0000011`), Store (`0100011`), Branch (`1100011`), LUI (`0110111`), AUIPC (`0010111`), JAL (`1101111`), JALR (`1100111`), SYSTEM/CSR (`1110011`).

---

### Day 5 — Pipeline Hazards & Forwarding (5-stage)

**Time**: ~4 hours

**Goal**: Trace instructions through IF/ID/EX/MEM/WB, identify hazards, determine forwarding paths and stall cycles.

#### Read

- P&H RISC-V Ed. (B1): Chapter 4, Sections 4.5–4.8
- OR: Berkeley CS61C lectures 20–23 (L2)
- OR: Onur Mutlu (L1) "Pipelining" + "Data Hazards" — skip his OoO lectures

#### Exercise 1: RAW Data Hazard with Forwarding

```asm
add  x3, x1, x2    # I0: produces x3
sub  x5, x3, x4    # I1: needs x3 — EX/MEM -> EX forward
and  x6, x3, x5    # I2: needs x3 (MEM/WB->EX), x5 (EX/MEM->EX)
or   x7, x6, x3    # I3: needs x6 (EX/MEM->EX), x3 (regfile, no forward)
```

Fill in:

| Cycle | IF | ID | EX | MEM | WB | Forwarding |
|---|---|---|---|---|---|---|
| 1 | I0 |    |    |     |    | — |
| 2 | I1 | I0 |    |     |    | — |
| 3 | I2 | I1 | I0 |     |    | — |
| 4 | I3 | I2 | I1 | I0  |    | I1.rs1(x3) <- EX/MEM |
| 5 |    | I3 | I2 | I1  | I0 | I2.rs1(x3) <- MEM/WB; I2.rs2(x5) <- EX/MEM |
| 6 |    |    | I3 | I2  | I1 | I3.rs1(x6) <- EX/MEM |
| 7 |    |    |    | I3  | I2 | — |
| 8 |    |    |    |     | I3 | — |

#### Exercise 2: Load-Use Hazard (mandatory 1-cycle stall)

```asm
lw   x3, 0(x1)     # I0
add  x5, x3, x4    # I1: needs x3 — load result not ready in time
sub  x6, x5, x7    # I2
```

Questions:
1. How many stall cycles? (1)
2. What happens during stall? (bubble injected into EX; IF and ID hold)
3. Which forwarding path delivers x3 after the stall? (MEM/WB -> EX)

Draw the full diagram including the bubble.

#### Exercise 3: Branch Hazard (static not-taken)

```asm
beq  x1, x2, target
add  x3, x4, x5      # fetched speculatively
sub  x6, x7, x8      # fetched speculatively
target:
or   x9, x10, x11
```

Branch resolved in EX. If taken: flush IF and ID (2 bubbles = 2-cycle penalty). If not taken: 0 penalty.
Draw both.

**Maps to**: `forwarding_net.v`, `hazard_unit.v`, `pipeline_ctrl.v`, `branch_unit.v`

#### Checkpoint

You can trace any 5-instruction sequence through the 5-stage pipeline, mark every forwarding path, and count stall cycles.

---

### Day 6 — RV32M: Multiplier & Divider

**Time**: ~4 hours

**Goal**: Understand how mul/div integrate into a 5-stage pipeline. This is a common interview follow-up ("how'd you handle multi-cycle EX?").

#### Read

- RISC-V Unprivileged Spec Ch 7 (M extension) — funct7=`0000001` distinguishes M from base R-type
- P&H RISC-V Ed. Ch 3, Sections 3.3–3.4 (hardware multiplier / divider algorithms)

#### Concepts

**Multiplier options**:
| Approach | Latency | Area | When to use |
|---|---|---|---|
| Single-cycle combinational (`a * b`) | 1 | Huge (DSP or many LUTs) | FPGA with DSP48 blocks — Vivado will infer |
| Iterative shift-add | 32 | Small | ASIC / area-constrained |
| Pipelined (3–4 stage) | 1 throughput, 3–4 latency | Medium | Best of both — this is what you build |

For Artix-7: infer a DSP48-based pipelined multiplier. Vivado does this if you write `wire [63:0] product = a * b;` inside a pipelined register chain.

**Divider**: iterative only (no easy DSP mapping). ~32 cycles for 32-bit. Must stall EX until done — signal `busy` back to `hazard_unit.v`.

#### Exercise: Design the mul/div interface

Sketch the module ports for `multiplier.v` and `divider.v`:
```
module multiplier (
    input         clk, reset,
    input  [31:0] a, b,
    input  [1:0]  op,        // MUL, MULH, MULHSU, MULHU
    input         start,
    output [31:0] result,
    output        done
);
```

How does `exec_stage.v` stall the pipeline when `divider.busy` is high? (Answer: `hazard_unit.v` asserts `stall_ex` while `busy`, which propagates to IF/ID hold + bubble insertion into MEM.)

#### Exercise: Trace 4 instructions with a mul

```asm
add  x1, x2, x3
mul  x4, x1, x5    # 3-cycle pipelined multiply
add  x6, x4, x7    # depends on x4
sub  x8, x6, x9
```

Where do stalls happen? Which forwarding paths fire? Draw it.

**Maps to**: `multiplier.v`, `divider.v`, `exec_stage.v`, `hazard_unit.v`

#### Checkpoint

You can explain the mul/div tradeoffs, sketch the pipeline integration, and trace a mixed IM sequence.

---

### Day 7 — Caches + CSRs & Traps

**Time**: ~4–5 hours

#### Part 1: Caches (2 hours)

**Read**: P&H RISC-V Ed. Ch 5, Sections 5.1–5.4.

Your cache config: 4KB, 2-way set-associative, 32-byte lines.
- Cache size = 4096 B
- Ways = 2
- Line size = 32 B
- Sets = 4096 / (2 × 32) = 64
- Offset bits = log2(32) = 5
- Index bits = log2(64) = 6
- Tag bits = 32 − 5 − 6 = 21

**Exercise: address breakdown**

| Address | Tag | Index | Offset | Set # |
|---|---|---|---|---|
| `0x00001234` | | | | |
| `0x00001254` | | | | |
| `0x00001834` | | | | |
| `0xDEADBEEF` | | | | |

Which pairs conflict (same set)?

**AMAT calculation**:
```
AMAT = hit_time + miss_rate × miss_penalty
```
If hit = 1 cycle, miss rate = 5%, miss penalty = 100 cycles → AMAT = 1 + 0.05 × 100 = 6 cycles.

**Maps to**: `hardware/rtl/cpu/cache/l1_icache.v`, `l1_dcache.v`.
For CPU bring-up: BRAM ports first, caches come later. Don't block the pipeline on them.

#### Part 2: CSRs & Traps (2 hours)

**Read**: RISC-V Privileged Spec Ch 3 (M-mode only).

**Minimum CSR set**:
| CSR | Addr | Purpose |
|---|---|---|
| mstatus | 0x300 | Global interrupt enable (MIE bit), previous MPP/MPIE on trap |
| mie | 0x304 | Interrupt enables (MTIE, MSIE, MEIE) |
| mtvec | 0x305 | Trap vector base — PC jumps here on trap |
| mepc | 0x341 | Saved PC on trap; PC to restore on `mret` |
| mcause | 0x342 | Trap cause (bit 31 = interrupt, low bits = code) |
| mtval | 0x343 | Bad address / illegal instr value |
| mip | 0x344 | Pending interrupts (read-only from most sources) |
| mcycle | 0xB00 | Cycle counter |
| minstret | 0xB02 | Instructions retired counter |

**Trap flow**:
```
1. Illegal instr / ecall / misaligned mem detected in EX or MEM
2. trap_unit asserts flush, redirects PC to mtvec
3. mepc = PC of trapping instruction
4. mcause = trap code
5. mstatus.MIE saved to MPIE, MIE cleared
6. mret in handler: PC <- mepc, MIE <- MPIE
```

**Exercise**: draw the datapath additions to your 5-stage pipeline to support traps. Which stage detects each trap type? (illegal instr → ID; misaligned load/store → MEM; ecall → ID or EX depending on decode; timer interrupt → asynchronous, checked at commit boundary).

**Maps to**: `csr_file.v`, `trap_unit.v`.

#### Checkpoint

You can decompose any address into tag/index/offset. You calculate AMAT. You can list the minimum CSR set and describe the trap flow end-to-end.

---

### Day 8 — GPU Architecture: SIMT Fundamentals

**Time**: ~5 hours

**Goal**: Understand modern GPU architecture — the SIMT model, warp scheduling, register file design, memory hierarchy. This is Phase 3 project material AND the highest-leverage interview topic for NVIDIA.

#### Read / Watch (in this order)

1. **PMPP (B2) Ch 1–4** — SIMT model, thread hierarchy, memory model. ~2 hours.
2. **NVIDIA CUDA Programming Guide Ch 5** (G2) — Performance Guidelines: coalescing, occupancy, warp divergence. 45 min.
3. **NVIDIA GTC — "How GPU Computing Works"** (L3) — visual reinforcement. 45 min.
4. **NVIDIA Volta whitepaper** (G3) — the first modern SM design with independent thread scheduling. 30 min skim.
5. **PMPP Ch 5–6** — memory hierarchy, coalescing in depth. 1 hour.

Save Ampere whitepaper + Volkov thesis for later in August alongside hls4ml.

#### Key Concepts You Must Own

**1. SIMT vs SIMD**
```
SIMD (CPU vector):  One instruction operates on a vector register.
                    ADD v0, v1, v2  -> v0[i]=v1[i]+v2[i] for each i.
                    Programmer explicitly uses vector instructions.

SIMT (GPU):         One instruction broadcast to N threads.
                    Each thread has its own registers and (conceptually) its own PC.
                    Hardware groups threads into warps and executes in lockstep.
                    Programmer writes scalar code; hardware parallelizes.
```

**2. Hierarchy**
```
GPU
+-- SM 0                                (our "GPU core" — one SM on FPGA)
|   +-- Warp Scheduler                  (picks a ready warp each cycle)
|   +-- Register File (banked per warp)
|   +-- Execution Lanes (4 on ours, 32 on NVIDIA)
|   |   +-- ALU, LSU, ...
|   +-- Shared Memory / Scratchpad
|   +-- Load/Store Unit (to global via AXI)
+-- SM 1 ...
+-- Global Memory (BRAM in our case)
```

**3. Warp Scheduling — Latency Hiding**

The insight: when a warp stalls (memory), the scheduler picks another ready warp. No pipeline bubble. This is how GPUs hide latency without OoO execution.

| Concept | NVIDIA | Our Design |
|---|---|---|
| Warp size | 32 threads | 4 threads (4 lanes) |
| Warps per SM | up to 64 | 4 |
| Scheduling | greedy / round-robin / age-based | round-robin |

**4. Warp Divergence**
```c
if (threadID < 8) {
    a = x + y;      // Path A: threads 0–7 active
} else {
    a = x - y;      // Path B: threads 8–15 active
}
```
Hardware:
1. All threads reach the `if`
2. Threads 0–7 run Path A; threads 8–15 masked off
3. Threads 8–15 run Path B; threads 0–7 masked off
4. Both paths finish → threads reconverge

Divergence halves throughput. Your GPU needs **active mask bits per warp** and a **reconvergence stack**.

**5. Memory Coalescing**
When all 32 threads in a warp access consecutive addresses (`base + tid * 4`), the hardware issues ONE 128-byte transaction. Random access → 32 separate transactions. This one concept alone can move a kernel 10× in either direction.

#### Exercise 1: Warp Scheduler on Paper

Given 4 warps:

| Warp | State | PC | Reason |
|---|---|---|---|
| W0 | RUNNING | 0x100 | executing this cycle |
| W1 | STALLED | 0x200 | waiting on memory |
| W2 | READY   | 0x150 | ready to run |
| W3 | DONE    | —     | finished |

- Round-robin: next cycle picks which warp?
- W1's memory read completes this cycle — new state?
- Draw the FSM: IDLE → READY → RUNNING → STALLED → READY, RUNNING → DONE.

#### Exercise 2: Divergence + Reconvergence Trace

For a warp of 4 threads running the `if/else` above, trace the active-mask per cycle. Where does the mask go into a reconvergence stack? When is it popped?

#### Exercise 3: Coalescing

Given `A[base + tid * stride]` where warp size = 4, base = 0x1000, threads issue in parallel. Draw the memory transactions issued for:
- stride = 1 (word-adjacent) — coalesced?
- stride = 4 — how many transactions?
- stride = 32 — worst case?

#### Study: Reference RTL

- **Simty (G5)** — start here. Small, synthesizable, comments are readable.
- **Vortex (G6)** — `hw/rtl/core/VX_warp_sched.sv`, `VX_fetch.sv`. Understand the module hierarchy; don't line-by-line read.

**Maps to**: `hardware/rtl/gpu/core/warp_scheduler.v`, `exec_lane.v`, `gpu_alu.v`, `gpu_regfile.v`, `gpu_scratchpad.v`.

#### Checkpoint

You can explain SIMT vs SIMD in one sentence each. You can trace warp scheduling, divergence + reconvergence, and coalescing. You've browsed Simty and can name the modules you'd write.

---

### Day 9 — Embedded C: Bare-Metal RISC-V + GPU MMIO

**Time**: ~4–5 hours

#### Part 1: Bare-Metal Startup & MMIO (2.5 hours)

**Read**: E1 ("RISC-V from scratch" Parts 1–3) OR E2 (Five EmbedDev).

**MMIO register access**:
```c
#include <stdint.h>

#define REG32(addr) (*(volatile uint32_t*)(addr))

#define UART_BASE       0x10000000
#define UART_TX_DATA    REG32(UART_BASE + 0x00)
#define UART_RX_DATA    REG32(UART_BASE + 0x04)
#define UART_STATUS     REG32(UART_BASE + 0x08)
#define UART_STATUS_TX_FULL  (1 << 0)
#define UART_STATUS_RX_VALID (1 << 1)

void uart_putc(char c) {
    while (UART_STATUS & UART_STATUS_TX_FULL);
    UART_TX_DATA = c;
}
void uart_puts(const char *s) { while (*s) uart_putc(*s++); }
```

**CSR access from C**:
```c
static inline uint32_t csr_read(int csr_num) {
    uint32_t val;
    asm volatile("csrr %0, %1" : "=r"(val) : "i"(csr_num));
    return val;
}
static inline void csr_write(int csr_num, uint32_t val) {
    asm volatile("csrw %0, %1" :: "i"(csr_num), "r"(val));
}
#define CSR_MSTATUS 0x300
#define CSR_MIE     0x304
#define CSR_MTVEC   0x305
#define CSR_MEPC    0x341
#define CSR_MCAUSE  0x342
```

**Interrupt setup**:
```c
extern void _trap_handler(void);
void setup_interrupts(void) {
    csr_write(CSR_MTVEC, (uint32_t)&_trap_handler);
    csr_write(CSR_MIE, csr_read(CSR_MIE) | (1 << 7));       // MTIE
    csr_write(CSR_MSTATUS, csr_read(CSR_MSTATUS) | (1 << 3)); // MIE
}
```

#### Exercise 1: startup.S (30 min)
```asm
.section .text.startup
.globl _start
_start:
    # 1. Set stack pointer to _stack_top
    # 2. Zero BSS from _bss_start to _bss_end
    # 3. Call main
    # 4. Infinite loop if main returns
```
**Maps to**: `firmware/boot/startup.S`

#### Exercise 2: trap_handler.S (30 min)
```asm
.section .text.trap
.globl _trap_handler
_trap_handler:
    # 1. Save ra, t0-t6, a0-a7 to stack
    # 2. Read mcause into a0
    # 3. Call trap_dispatch(mcause)
    # 4. Restore registers
    # 5. mret
```
**Maps to**: `firmware/boot/trap_handler.S`

#### Part 2: CPU↔GPU Memory Map (1.5 hours)

**SoC memory map**:
```
0x0000_0000 – 0x0000_FFFF : Instruction BRAM (64 KB)
0x0001_0000 – 0x0003_FFFF : Data BRAM (192 KB)
0x1000_0000 – 0x1000_000F : UART
0x1000_1000 – 0x1000_100F : Timer / CLINT
0x1000_2000 – 0x1000_200F : GPIO
0x2000_0000 – 0x2000_003F : GPU command registers
0x3000_0000 – 0x3000_FFFF : GPU shared memory
```

**Exercise 3: GPU driver (30 min)**
```c
#define GPU_BASE         0x20000000
#define GPU_STATUS       REG32(GPU_BASE + 0x00)  // [0]=busy, [1]=done, [2]=error
#define GPU_CMD          REG32(GPU_BASE + 0x04)
#define GPU_KERNEL_ADDR  REG32(GPU_BASE + 0x08)
#define GPU_DATA_ADDR    REG32(GPU_BASE + 0x0C)
#define GPU_RESULT_ADDR  REG32(GPU_BASE + 0x10)
#define GPU_NUM_THREADS  REG32(GPU_BASE + 0x14)

void gpu_launch_kernel(uint32_t kernel, uint32_t data, uint32_t result, uint32_t threads) {
    while (GPU_STATUS & 0x1);
    GPU_KERNEL_ADDR = kernel;
    GPU_DATA_ADDR   = data;
    GPU_RESULT_ADDR = result;
    GPU_NUM_THREADS = threads;
    GPU_CMD = 1;
}
void gpu_wait(void) { while (!(GPU_STATUS & 0x2)); }
```
**Maps to**: `firmware/drivers/gpu.c`, `hardware/rtl/gpu/gpu_cmd_proc.v`.

#### Checkpoint

You can write MMIO driver code with `volatile`. You can write startup asm and a trap handler. You know the memory map cold.

---

### Day 10 — Integration: Ship Your First Module

**Time**: ~3–4 hours

**Goal**: Prove readiness. Write real project code, run a real testbench, watch it pass.

#### Task 1: Implement `regfile.v` (1 hour)

Spec:
- 32 × 32 bits
- 2 read ports (async), 1 write port (sync posedge clk)
- x0 hardwired to zero (reads return 0, writes ignored)
- Synchronous reset zeros everything

```verilog
module regfile (
    input         clk,
    input         reset,
    input  [4:0]  rs1_addr,
    input  [4:0]  rs2_addr,
    input  [4:0]  rd_addr,
    input  [31:0] rd_data,
    input         rd_write_en,
    output [31:0] rs1_data,
    output [31:0] rs2_data
);
```

#### Task 2: Run the testbench (30 min)

```bash
cd hardware/sim
chmod +x run_sim.sh
./run_sim.sh cpu/tb_regfile
```

Pass → move on. Fail → `gtkwave waves/tb_regfile.vcd`.

#### Task 3: Implement `imm_gen.v` (1 hour)

```verilog
module imm_gen (
    input  [31:0] instruction,
    input  [2:0]  imm_type,    // 0=I, 1=S, 2=B, 3=U, 4=J
    output reg [31:0] immediate
);
// Sign-extend immediates per format. See RISC-V spec Fig 2.4.
endmodule
```

#### Task 4: Re-read `docs/cpu_design.md` bring-up order

The 10-step order in `docs/cpu_design.md` is your next-two-weeks plan. Read it now that every concept in it makes sense.

#### Checkpoint

`regfile.v` compiles and passes tb. `imm_gen.v` handles all 5 immediate formats. You know exactly which file you're writing tomorrow.

---

## Quick-Reference: What to Study When Stuck

| Stuck on... | Go to... |
|---|---|
| Verilog syntax | HDLBits (P1), FPGA Prototyping (B3) |
| Instruction encoding | Green Card (R3), rvcodecjs (R4) |
| Pipeline hazards | P&H Ch 4 (B1), Day 5 exercises |
| Multiplier/divider integration | P&H Ch 3.3–3.4 (B1), Day 6 |
| Cache design | P&H Ch 5 (B1), Day 7 Part 1 |
| CSRs / traps | Privileged Spec Ch 3 (R2), Day 7 Part 2 |
| GPU / SIMT / warps | PMPP (B2), CUDA Guide Ch 5 (G2), Day 8 |
| Warp scheduling / divergence | Volta whitepaper (G3), Simty (G5), Day 8 |
| Coalescing / occupancy | CUDA Guide Ch 5 (G2), Volkov thesis (G4) |
| Bare-metal C | E1, E2 |
| Verilator testbenches | S1, existing `tb_alu.cpp` in project |
| Compliance failures | riscv-arch-test README (V1), `docs/compliance_results.md` |
