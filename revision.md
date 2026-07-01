# Omni-RISC APU — 10-Day Revision Plan

> Revision plan covering Verilog, RISC-V ISA, CPU architecture (OoO, branch prediction,
> caches), GPU architecture (SIMT, warp scheduling), and embedded C bare-metal programming.
> Every exercise maps to a specific file in this project.

---

## Master Resource Table

### Books (You Should Own These)

| # | Book | Author | What You Need From It |
|---|------|--------|----------------------|
| B1 | Computer Organization and Design: RISC-V Edition | Patterson & Hennessy | Ch 4: Pipeline, hazards, forwarding. Ch 5: Caches. Green Card: instruction encoding |
| B2 | Computer Architecture: A Quantitative Approach (6th Ed) | Hennessy & Patterson | Ch 3: OoO execution, Tomasulo, ROB, branch prediction. Ch 4: GPU architecture, SIMT |
| B3 | FPGA Prototyping by Verilog Examples | Pong P. Chu | Verilog coding patterns for FPGA, BRAM inference, UART, SPI |

### Online Courses & Lectures

| # | Resource | Link | Topics |
|---|----------|------|--------|
| L1 | **Onur Mutlu — Digital Design & Computer Architecture (ETH Zürich)** | https://www.youtube.com/@OnurMutluLectures | OoO execution, branch prediction, memory hierarchy, GPU architecture. Best lecture series for this project. Search his channel for specific topics. |
| L2 | **Georgia Tech CS 6290 — High-Performance Computer Architecture** | Search "CS 6290 HPCA" on YouTube/Udacity | OoO, Tomasulo, ROB, superscalar, caches. More industry-focused than Mutlu. |
| L3 | **MIT 6.823 — Computer System Architecture** | Search "MIT 6.823 lecture notes" | Practical ROB/RAT implementation, scoreboard vs Tomasulo tradeoffs. Lecture notes are gold. |

### Interactive Practice

| # | Resource | Link | Topics |
|---|----------|------|--------|
| P1 | **HDLBits** | https://hdlbits.01xz.net/wiki/Problem_sets | Verilog exercises with instant feedback. The single best Verilog practice site. |
| P2 | **ChipDev** | https://chipdev.io/question-list | Verilog & VLSI interview questions. Harder than HDLBits. |

### RISC-V References

| # | Resource | Link | Topics |
|---|----------|------|--------|
| R1 | **RISC-V ISA Spec (Unprivileged)** | https://riscv.org/specifications/ | RV32IM instruction encoding, all formats |
| R2 | **RISC-V Privileged Spec** | https://riscv.org/specifications/ | CSRs, interrupts, traps, mstatus/mtvec/mepc/mcause |
| R3 | **RISC-V Green Card** | Included with P&H textbook; also search "RISC-V reference card PDF" | Quick-reference for opcode, funct3, funct7. Print this. Keep it on your desk. |
| R4 | **RISC-V Instruction Encoder/Decoder** | https://luplab.gitlab.io/rvcodecjs/ | Online tool to verify your hand-encoding exercises |

### GPU Architecture

| # | Resource | Link | Topics |
|---|----------|------|--------|
| G1 | **Hennessy & Patterson Quantitative Approach, Ch 4** | Your textbook | GPU architecture fundamentals, SIMT model, thread hierarchy |
| G2 | **NVIDIA CUDA C++ Programming Guide — Ch 4: Hardware Implementation** | https://docs.nvidia.com/cuda/cuda-c-programming-guide/ | SM internals, warp scheduling, register file, shared memory, divergence |
| G3 | **Vortex GPGPU (GitHub)** | https://github.com/vortexgpgpu/vortex | Full open-source RISC-V GPGPU in Verilog. Study the RTL structure. |
| G4 | **Vortex Tutorials** | https://github.com/vortexgpgpu/vortex_tutorials | Conference tutorial slides + hands-on exercises covering SIMT microarchitecture |
| G5 | **"How GPU Computing Works" — GTC Talk** | Search "How GPU Computing Works NVIDIA GTC" on YouTube | Visual walkthrough of SM architecture, warp execution, memory hierarchy |
| G6 | **Onur Mutlu — GPU Architecture Lectures** | Search "Onur Mutlu GPU" on his YouTube channel | SIMT vs SIMD, warp scheduling, GPU memory hierarchy, divergence handling |
| G7 | **FGPU Project** | Search "FGPU FPGA GPU" on GitHub | Simpler FPGA GPU in VHDL, good for understanding minimal SIMT design |
| G8 | **Steven Gong — "How GPUs Work" Blog** | https://stevengong.co | Clear visual explanations of SM internals and warp scheduling |

### Embedded C / Bare-Metal

| # | Resource | Link | Topics |
|---|----------|------|--------|
| E1 | **"RISC-V from scratch" Blog** | Search "twilco RISC-V from scratch" on GitHub Pages | Startup assembly, linker scripts, bare-metal boot. 3-part series. |
| E2 | **Five EmbedDev** | https://five-embeddev.com | RISC-V bare-metal C, interrupt handling, CSR access |
| E3 | **Vociferous Void — RISC-V Bare Metal** | Search "vociferousvoid RISC-V bare metal" | Structured tutorial: boot → UART → interrupts → timer |

### Verilator & Simulation

| # | Resource | Link | Topics |
|---|----------|------|--------|
| S1 | **Verilator User Guide** | https://verilator.org/guide/latest/ | C++ testbench API, tracing, --trace flag |
| S2 | **ZipCPU Verilator Tutorial** | Search "ZipCPU Verilator tutorial" | Practical examples of Verilator testbenches |

---

## Day-by-Day Plan

---

### Day 1 — Verilog: Combinational Logic Refresh

**Time**: ~4 hours

**Goal**: Recover your Verilog muscle memory for combinational circuits. After today you should be able to write any combinational module from scratch without looking up syntax.

#### HDLBits Exercises (Do All)

Go to https://hdlbits.01xz.net/wiki/Problem_sets and complete:

| Category | Problems | Est. Time | Why |
|----------|----------|-----------|-----|
| Getting Started → Step One | 1 problem | 2 min | Remember `module`, `endmodule`, `assign` |
| Verilog Language → Basics | Wire, Wire4, Notgate, Andgate, Norgate, Xnorgate, Wire_decl | 15 min | Basic syntax recovery |
| Verilog Language → Vectors | Vector0 through Vector5, Vectorgates, Gates4 | 20 min | Bit slicing, concatenation — used everywhere |
| Verilog Language → Modules: Hierarchy | Module, Module_pos, Module_name, Module_shift, Module_shift8, Module_cseladd, Module_addsub | 30 min | You'll instantiate ~20 submodules in cpu_top.v |
| Circuits → Combinational Logic → Basic Gates | All available | 15 min | Quick wins to build confidence |
| Circuits → Combinational Logic → Multiplexers | Mux2to1, Mux2to1v, Mux9to1v, Mux256to1, Mux256to1v | 20 min | Your ALU, forwarding mux, PC mux are all muxes |
| Circuits → Combinational Logic → Arithmetic | Hadd, Fadd, Adder, Signed addition overflow | 20 min | ALU internals |
| Circuits → Combinational Logic → Karnaugh Maps | All available | 20 min | Decode logic optimization |

#### Self-Test Exercise (30 min)

Write from scratch (no references):

```
A parameterized N-bit ALU that supports:
  - ADD, SUB, AND, OR, XOR, SLT, SLTU, SLL, SRL, SRA
  - Input:  [N-1:0] a, b; [3:0] op
  - Output: [N-1:0] result; zero_flag
Use a case statement on op.
```

**Maps to project file**: `hardware/rtl/cpu/core/alu.v`
**Test it with**: `hardware/sim/cpu/tb_alu.cpp`

#### Checkpoint

✅ You're ready to move on when you can write a `case`-based mux, an `assign`-based mux, and a parameterized module header from memory. You know the difference between `wire`, `reg`, `logic`, `assign`, `always @(*)`, and `always @(posedge clk)`.

---

### Day 2 — Verilog: Sequential Logic + FSMs

**Time**: ~4–5 hours

**Goal**: Master clocked logic and state machines. Almost every module in the CPU is sequential.

#### HDLBits Exercises (Do All)

| Category | Problems | Est. Time | Why |
|----------|----------|-----------|-----|
| Circuits → Sequential Logic → Latches and Flip-Flops | Dff, Dff8, Dff8r, Dff8p, Dff8ar, Dff16e | 20 min | Every pipeline register is a D flip-flop |
| Circuits → Sequential Logic → Counters | Count15, Count10, Count1to10, Countslow, all others | 25 min | Cycle counters, timer, ROB head/tail pointers |
| Circuits → Sequential Logic → Shift Registers | Shift4, Rotate100, Shift18, Lfsr5, all others | 25 min | GShare Global History Register is a shift register |
| Circuits → Sequential Logic → More Circuits | Rule90, Rule110, Conway's Game of Life (if available) | 30 min | Complex sequential logic patterns |
| Circuits → Sequential Logic → FSMs | Fsm1, Fsm1s, Fsm2, Fsm2s, Fsm3comb, Fsm3onehot, Fsm3, Lemmings1-4, Fsm_serial, Fsm_serialdata, Fsm_ps2 | 60 min | **Critical** — your CPU control, UART, GPU cmd processor are all FSMs |
| Circuits → Building Larger Circuits | Counter with period 1000, FSM: Sequence recognizer, Timer | 30 min | Integration practice |

#### Self-Test Exercise: UART Transmitter FSM (45 min)

Write a complete UART TX module from scratch:

```
module uart_tx (
    input  clk, reset,
    input  [7:0] tx_data,
    input  tx_valid,         // pulse to start transmission
    output reg tx_out,       // serial output line
    output     tx_busy       // high while transmitting
);
// States: IDLE → START_BIT → DATA[0..7] → STOP_BIT → IDLE
// Use a baud rate counter (parameter CLKS_PER_BIT)
// 2-block FSM style: one always for state reg, one for next-state logic
endmodule
```

**Maps to project file**: `hardware/rtl/peripherals/uart.v`

#### Key Patterns to Memorize

**2-block FSM style** (use this everywhere):
```verilog
// Block 1: State register
always @(posedge clk or posedge reset) begin
    if (reset) state <= IDLE;
    else       state <= next_state;
end

// Block 2: Next-state + output logic
always @(*) begin
    next_state = state; // default: stay
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
    // if stall: hold current value (implicit)
end
```

#### Checkpoint

✅ You can write a 5+ state Moore FSM using 2-block style without references. You understand `posedge clk`, synchronous vs asynchronous reset, and the stall/flush pipeline register pattern.

---

### Day 3 — Verilog: Memories, Parameters & Testbenches

**Time**: ~4 hours

**Goal**: Understand BRAM inference, parameterized modules, `$readmemh`, and Verilator C++ testbench pattern.

#### Study: BRAM Inference Patterns

Xilinx FPGAs infer Block RAM vs distributed RAM based on how you code:

```verilog
// BRAM (synchronous read) — used for ROB, caches, scratchpad
(* ram_style = "block" *) reg [31:0] mem [0:1023];
always @(posedge clk) begin
    if (wen) mem[waddr] <= wdata;
    rdata <= mem[raddr];  // synchronous read → BRAM
end

// Distributed RAM (async read) — used for register file, RAT
reg [31:0] mem [0:31];
always @(posedge clk) begin
    if (wen) mem[waddr] <= wdata;
end
assign rdata = mem[raddr];  // async read → distributed (LUT) RAM
```

**Why this matters**: Your ROB (16 entries × 80 bits) should be BRAM. Your register file (32 × 32 bits) should be distributed RAM (for combinational reads in same cycle as decode).

#### Self-Test Exercises

**Exercise 1: Parameterized register file** (20 min)
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
// 2 async read ports, 1 sync write port
// x0 hardwired to zero
endmodule
```

**Maps to project file**: `hardware/rtl/cpu/core/regfile.v`

**Exercise 2: Simple direct-mapped cache** (30 min)
```
- 1KB, direct-mapped, 16-byte lines
- Ports: addr, rdata, wdata, ren, wen, hit, miss
- Tag + valid bit array
- On hit: return data
- On miss: assert miss signal (external fill logic handles it)
```

**Maps to project files**: `hardware/rtl/cpu/cache/l1_icache.v`, `hardware/rtl/cpu/cache/l1_dcache.v`

#### Verilator Testbench Refresher

Read and understand the existing testbench:
- Open `hardware/sim/cpu/tb_alu.cpp` — study the pattern
- Open `hardware/sim/run_sim.sh` — understand the compilation flow

**Key Verilator C++ pattern**:
```cpp
#include "Vdut_name.h"
#include "verilated.h"
#include "verilated_vcd_c.h"

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);
    Vdut_name* dut = new Vdut_name;

    // Optional: VCD tracing
    Verilated::traceEverOn(true);
    VerilatedVcdC* tfp = new VerilatedVcdC;
    dut->trace(tfp, 99);
    tfp->open("waves/trace.vcd");

    // Clock loop
    for (int cycle = 0; cycle < 1000; cycle++) {
        dut->clk = 0; dut->eval(); tfp->dump(cycle*2);
        dut->clk = 1; dut->eval(); tfp->dump(cycle*2+1);

        // Check outputs here
    }

    tfp->close();
    delete dut;
    return 0;
}
```

#### Checkpoint

✅ You know the difference between BRAM and distributed RAM inference. You can write a Verilator C++ testbench. You understand `$readmemh` and `$clog2`.

---

### Day 4 — RISC-V ISA: Instruction Encoding

**Time**: ~4 hours

**Goal**: Be able to hand-encode and hand-decode any RV32IM instruction. This directly becomes your `decoder.v`.

#### Read

- RISC-V Green Card (print it, keep it on your desk forever)
- RISC-V Unprivileged Spec: Chapter 2 (RV32I), Chapter 7 (M extension)

#### Exercise 1: Encode These Instructions (Pen & Paper)

Use the Green Card. For each, write the full 32-bit binary and hex encoding.

| # | Instruction | Type | Binary (fill in) | Hex |
|---|---|---|---|---|
| 1 | `add  x3, x1, x2` | R | `0000000_00010_00001_000_00011_0110011` | `0x002081B3` |
| 2 | `sub  x5, x3, x4` | R | | |
| 3 | `and  x6, x7, x8` | R | | |
| 4 | `mul  x3, x1, x2` | R | | |
| 5 | `div  x4, x5, x6` | R | | |
| 6 | `addi x1, x0, 42` | I | | |
| 7 | `slli x3, x1, 5` | I | | |
| 8 | `lw   x7, 8(x2)` | I | | |
| 9 | `lb   x3, -1(x5)` | I | | |
| 10 | `sw   x7, 12(x2)` | S | | |
| 11 | `sh   x3, 6(x4)` | S | | |
| 12 | `beq  x1, x2, +16` | B | | |
| 13 | `bne  x3, x0, -8` | B | | |
| 14 | `blt  x1, x2, +32` | B | | |
| 15 | `lui  x5, 0xDEADB` | U | | |
| 16 | `auipc x6, 0x10` | U | | |
| 17 | `jal  x1, +100` | J | | |
| 18 | `jalr x1, x5, 0` | I | | |
| 19 | `csrrw x3, mstatus, x1` | I (CSR) | | |
| 20 | `ecall` | I | | |

**Verify your answers**: Use https://luplab.gitlab.io/rvcodecjs/

#### Exercise 2: Decode These Hex Values

| Hex | Decode to assembly |
|---|---|
| `0x002081B3` | `add x3, x1, x2` (example) |
| `0x00A12023` | |
| `0xFE5290E3` | |
| `0x00500113` | |
| `0xDEADB2B7` | |
| `0x0000006F` | |
| `0x02208233` | |

#### Key Insight for Your Decoder

When you write `decoder.v`, the decode logic is literally:
```verilog
wire [6:0] opcode = instruction[6:0];
wire [2:0] funct3 = instruction[14:12];
wire [6:0] funct7 = instruction[31:25];
wire [4:0] rd     = instruction[11:7];
wire [4:0] rs1    = instruction[19:15];
wire [4:0] rs2    = instruction[24:20];
```
The fields are ALWAYS in the same bit positions. This is a core RISC-V design principle.

**Maps to project files**: `hardware/rtl/cpu/core/decoder.v`, `hardware/rtl/cpu/core/imm_gen.v`

#### Checkpoint

✅ You can look at `0xFE5290E3` and decode it in under 2 minutes. You know all 6 instruction formats by heart. You know the opcodes for R-type (`0110011`), I-type ALU (`0010011`), Load (`0000011`), Store (`0100011`), Branch (`1100011`), LUI (`0110111`), AUIPC (`0010111`), JAL (`1101111`), JALR (`1100111`).

---

### Day 5 — RISC-V: Pipeline Hazards & Forwarding

**Time**: ~4 hours

**Goal**: Trace instruction sequences through a 5-stage pipeline, identify all hazards, and determine forwarding paths.

#### Read

- P&H RISC-V Ed. (B1): Chapter 4, Sections 4.5–4.8
- OR: Watch Onur Mutlu (L1) lectures on "Pipelining" and "Data Hazards"

#### Exercise 1: Data Hazard with Forwarding

Draw the pipeline diagram for this sequence. Mark all forwarding paths.

```asm
add  x3, x1, x2    # I0: produces x3
sub  x5, x3, x4    # I1: needs x3 — EX→EX forward
and  x6, x3, x5    # I2: needs x3 (MEM→EX), x5 (EX→EX)
or   x7, x6, x3    # I3: needs x6 (EX→EX), x3 (from register file — no forward needed)
```

| Cycle | IF | ID | EX | MEM | WB | Forwarding |
|---|---|---|---|---|---|---|
| 1 | I0 | | | | | |
| 2 | I1 | I0 | | | | |
| 3 | I2 | I1 | I0 | | | |
| 4 | I3 | I2 | I1 | I0 | | I1.rs1(x3) ← EX/MEM.result |
| 5 | | I3 | I2 | I1 | I0 | I2.rs1(x3) ← MEM/WB.result, I2.rs2(x5) ← EX/MEM.result |
| ... | | | | | | Continue... |

#### Exercise 2: Load-Use Hazard (Mandatory Stall)

```asm
lw   x3, 0(x1)     # I0: x3 loaded from memory
add  x5, x3, x4    # I1: needs x3 — data not available until end of MEM stage!
sub  x6, x5, x7    # I2
```

Questions:
1. How many stall cycles are inserted? (Answer: 1)
2. What does the pipeline do during the stall? (Answer: bubble in EX, hold IF+ID)
3. After the stall, what forwarding path delivers x3 to I1? (Answer: MEM→EX)

Draw the full pipeline diagram including the bubble.

#### Exercise 3: Branch Hazard

```asm
beq  x1, x2, target  # I0: branch resolved in EX
add  x3, x4, x5      # I1: fetched speculatively
sub  x6, x7, x8      # I2: fetched speculatively
target:
or   x9, x10, x11    # I3: actual target
```

If branch is **taken**: I1 and I2 are flushed. 2-cycle penalty (with branch resolved in EX).
If branch is **not taken**: no penalty.

Draw both scenarios.

**Maps to project files**: `hardware/rtl/cpu/core/forwarding_net.v`, `hardware/rtl/cpu/core/pipeline_ctrl.v`

#### Checkpoint

✅ You can trace any 5-instruction sequence through a 5-stage pipeline, identify RAW/WAW/WAR hazards, determine which forwarding paths fire, and calculate stall cycles.

---

### Day 6 — Architecture: Out-of-Order Execution

**Time**: ~5 hours (this is the hardest day)

**Goal**: Understand ROB, RAT, issue queues, and scoreboard scheduling. Trace OoO execution on paper.

#### Watch / Read

- **Onur Mutlu** (L1): Search his channel for "Out-of-Order Execution" — watch the 2-lecture series (~2.5 hours total)
- **OR** H&P Quantitative Approach (B2): Chapter 3, Sections 3.1–3.6
- **MIT 6.823** (L3) lecture notes on ROB and register renaming

#### Key Concepts to Understand

**1. The Three Phases of OoO:**
```
DECODE (In-Order) → EXECUTE (Out-of-Order) → COMMIT (In-Order)
     ↑                                            ↑
  Allocate ROB entry                         Retire from ROB head
  Update RAT                                 Write to arch. regfile
```

**2. Register Alias Table (RAT)**: Maps architectural registers → ROB entries
- On decode: `RAT[rd] = ROB_tag` (register will be produced by this ROB entry)
- On commit: If `RAT[rd]` still points to this ROB entry → mark as "architectural"

**3. Reorder Buffer (ROB)**: Circular buffer tracking in-flight instructions
- Allocate at tail (decode), complete in any order, commit from head (in order)
- Guarantees precise exceptions: if exception at ROB head, flush everything after it

**4. Scoreboard**: Tracks operand readiness for each instruction in the issue queue
- Each source operand: `{ready, value_or_tag}`
- When an FU broadcasts `(tag, result)`, all matching sources wake up

#### Exercise: Full OoO Trace (This is Critical)

Machine config: 2-wide issue, 4-entry ROB, 2 ALUs (Port A + Port B), MUL latency = 3 cycles.

```asm
I0: ADD  x3, x1, x2    # 1-cycle, Port A or B
I1: MUL  x4, x3, x5    # 3-cycle, Port B only, depends on x3
I2: SUB  x6, x7, x8    # 1-cycle, independent
I3: ADD  x9, x6, x10   # 1-cycle, depends on x6
I4: LW   x11, 0(x4)    # depends on x4 (MUL result)
I5: ADD  x12, x11, x9  # depends on x11 and x9
```

Fill in this table:

| Cycle | Decode (ROB alloc, RAT update) | Issue (scoreboard ready?) | Execute | Complete (result broadcast) | Commit (ROB head retire) |
|---|---|---|---|---|---|
| 1 | I0→ROB[0], I1→ROB[1] | | | | |
| 2 | I2→ROB[2], I3→ROB[3] | I0 (ready), I2 (ready) | | | |
| 3 | I4→ROB[0*], I5→ROB[1*] | I3 (x6 woke up), I1 (x3 woke up) | I0→ALU.A, I2→ALU.B | | |
| 4 | | | I3→ALU.A, I1→MUL | I0 done, I2 done | I0 commits |
| 5 | | | | I3 done | I1 waits (not done yet) |
| 6 | | | | I1 MUL running... | |
| 7 | | I4 (x4 woke up) | | I1 done | I1 commits |
| 8 | | I5 waits (x11?) | I4→LSU | I4 done? (cache hit) | I2 commits, I3 commits |
| ... | | | | | Continue... |

*Complete this trace yourself. Get comfortable with the mechanics.*

**Maps to project files**: `hardware/rtl/cpu/core/rob.v`, `hardware/rtl/cpu/core/rename_rat.v`, `hardware/rtl/cpu/core/scoreboard.v`, `hardware/rtl/cpu/core/issue_queue.v`, `hardware/rtl/cpu/core/wakeup_logic.v`

#### Checkpoint

✅ You can trace a 6-instruction OoO execution through a 2-wide machine with ROB, RAT, and scoreboard without getting confused. You understand why commit is in-order even though execution is out-of-order.

---

### Day 7 — Architecture: Branch Prediction + Caches

**Time**: ~3 hours

#### Part 1: GShare Branch Predictor (1.5 hours)

**Read**: McFarling 1993 paper "Combining Branch Predictors" (search for PDF — it's the original GShare paper, very readable).

**How GShare works**:
```
index = XOR(PC[11:2], Global_History_Register[9:0])
prediction = PHT[index]   // 2-bit saturating counter
             ≥ 2 → predict TAKEN
             < 2 → predict NOT TAKEN
```

**Exercise: Trace GShare State**

PHT starts at all 01 (weakly-not-taken). GHR starts at 0000000000.

A loop branch at PC = 0x100 executes with outcomes: T, T, T, T, T, T, T, T, N, T (repeat).

| Iteration | GHR (before) | XOR index | PHT[idx] before | Prediction | Actual | Correct? | PHT[idx] after | GHR (after) |
|---|---|---|---|---|---|---|---|---|
| 1 | 0000000000 | XOR(0x40, 0x000) = ? | 01 (WNT) | NT | T | ❌ Miss | 10 (WT) | 0000000001 |
| 2 | 0000000001 | ? | ? | ? | T | ? | ? | ? |
| ... | | | | | | | | |

*Complete for 10 iterations. Count total mispredicts.*

**Maps to project file**: `hardware/rtl/cpu/core/gshare_bpu.v`

#### Part 2: Cache Math (1.5 hours)

**Read**: P&H RISC-V Ed. (B1) Chapter 5, Sections 5.1–5.4.

**Your cache config**: 4KB, 2-way set-associative, 32-byte lines.

Calculations:
- Cache size = 4096 bytes
- Num ways = 2
- Line size = 32 bytes
- Num sets = 4096 / (2 × 32) = 64 sets
- Offset bits = log2(32) = 5
- Index bits = log2(64) = 6
- Tag bits = 32 - 5 - 6 = 21

**Exercise: Cache Address Breakdown**

For each address, identify tag, index, offset:

| Address | Tag (21 bits) | Index (6 bits) | Offset (5 bits) | Set # |
|---|---|---|---|---|
| `0x00001234` | | | | |
| `0x00001254` | | | | |
| `0x00001834` | | | | |
| `0xDEADBEEF` | | | | |

Which pairs of addresses above conflict (map to same set)?

**Maps to project files**: `hardware/rtl/cpu/cache/l1_icache.v`, `hardware/rtl/cpu/cache/l1_dcache.v`

#### Checkpoint

✅ You can trace GShare state through a branch sequence. You can decompose any 32-bit address into tag/index/offset for your cache configuration.

---

### Day 8 — GPU Architecture: SIMT Fundamentals

**Time**: ~5 hours

**Goal**: Understand how GPUs work at the hardware level — the SIMT model, warp scheduling, register file design, and shared memory. This is critical for Phase 3 of the project.

#### Watch / Read (Pick 2–3)

1. **Onur Mutlu — GPU Architecture Lecture** (L1, search "GPU" on his channel) — ~1.5 hours
2. **H&P Quantitative Approach Ch 4** (B2) — GPU architecture section — 1 hour reading
3. **NVIDIA CUDA Programming Guide, Ch 4: Hardware Implementation** (G2) — 30 min
4. **"How GPU Computing Works" GTC Talk** (G5) — 30 min video

#### Key Concepts You Must Understand

**1. SIMT vs SIMD — What's the Difference?**
```
SIMD (CPU vector):  One instruction operates on a vector register
                    ADD v0, v1, v2  → v0[0]=v1[0]+v2[0], v0[1]=v1[1]+v2[1], ...
                    Programmer explicitly uses vector instructions

SIMT (GPU):         One instruction broadcast to N threads
                    Each thread has its own registers, its own PC (conceptually)
                    Hardware groups threads into warps and executes in lockstep
                    Programmer writes scalar code, hardware parallelizes
```

**2. The GPU Hierarchy**
```
GPU
├── Streaming Multiprocessor (SM) 0     ← Our "GPU core" — one SM is enough for FPGA
│   ├── Warp Scheduler                  ← Picks which warp runs this cycle
│   ├── Register File (banked per warp) ← Each warp has its own register space
│   ├── Execution Lanes (4 in our design) ← SIMT lanes, all execute same instruction
│   │   ├── ALU                         ← INT arithmetic per lane
│   │   └── ...
│   ├── Shared Memory / Scratchpad      ← Fast per-SM memory, shared across warps
│   └── Load/Store Unit                 ← Memory access (scratchpad or global via AXI)
├── SM 1
│   └── ...
└── Global Memory (BRAM in our case)
```

**3. Warp Scheduling — Why It Matters**

| Concept | NVIDIA (real GPU) | Our Design (FPGA) |
|---|---|---|
| Warp size | 32 threads | 4 threads (4 lanes) |
| Warps per SM | Up to 64 | 4 warps |
| Scheduling policy | Greedy/round-robin | Round-robin |
| Latency hiding | Switch warp on memory stall | Same — switch to another warp while one waits |

**Key insight**: When a warp stalls (e.g., waiting for memory), the scheduler picks another ready warp. No pipeline bubble! This is how GPUs hide latency without OoO execution.

**4. Warp Divergence**
```c
// GPU "kernel" (runs on each thread)
if (threadID < 8) {
    a = x + y;      // Path A: threads 0–7
} else {
    a = x - y;      // Path B: threads 8–15
}
```
In SIMT hardware:
1. All threads reach the `if`
2. Threads 0–7 execute Path A, threads 8–15 are **masked off** (idle)
3. Then threads 8–15 execute Path B, threads 0–7 are masked off
4. Both paths done → all threads **reconverge**

This is called **divergence** and it halves throughput. Your GPU design must handle this with **active mask bits** per warp.

#### Exercise: Design Your GPU's Warp Scheduler (On Paper)

Given 4 warps with these states:

| Warp | State | PC | Reason |
|---|---|---|---|
| W0 | RUNNING | 0x100 | Currently executing |
| W1 | STALLED | 0x200 | Waiting for memory read |
| W2 | READY | 0x150 | Has instructions ready |
| W3 | DONE | — | Finished execution |

1. What does the round-robin scheduler do next cycle?
2. W1's memory read completes this cycle. Updated state table?
3. Draw the FSM for warp states: `IDLE → READY → RUNNING → STALLED → READY` and `RUNNING → DONE`

#### Study: Vortex GPGPU Source Structure

Browse https://github.com/vortexgpgpu/vortex — look at:
- `hw/rtl/core/` — how they structure fetch, decode, execute, memory for SIMT
- `hw/rtl/core/VX_warp_sched.sv` — warp scheduler implementation
- Don't read every line — just understand the module hierarchy and signal flow

**Maps to project files**: `hardware/rtl/gpu/core/warp_scheduler.v`, `hardware/rtl/gpu/core/exec_lane.v`, `hardware/rtl/gpu/core/gpu_alu.v`, `hardware/rtl/gpu/core/gpu_regfile.v`, `hardware/rtl/gpu/core/gpu_scratchpad.v`

#### Checkpoint

✅ You can explain SIMT vs SIMD. You understand warp scheduling, divergence, and masking. You can draw the GPU memory hierarchy (registers → scratchpad → global memory). You've browsed Vortex and understand how an FPGA GPU is structured.

---

### Day 9 — Embedded C: Bare-Metal RISC-V + GPU Memory Map

**Time**: ~4–5 hours

#### Part 1: Bare-Metal Startup & MMIO (2.5 hours)

**Read** (pick one):
- "RISC-V from scratch" blog Parts 1–3 (E1) — ~1.5 hours
- Five EmbedDev RISC-V bare-metal guide (E2)

**Key Pattern 1: MMIO Register Access**
```c
#include <stdint.h>

// Memory-mapped register access macro
#define REG32(addr) (*(volatile uint32_t*)(addr))

// UART at 0x10000000
#define UART_BASE       0x10000000
#define UART_TX_DATA    REG32(UART_BASE + 0x00)
#define UART_RX_DATA    REG32(UART_BASE + 0x04)
#define UART_STATUS     REG32(UART_BASE + 0x08)
#define UART_STATUS_TX_FULL  (1 << 0)
#define UART_STATUS_RX_VALID (1 << 1)

void uart_putc(char c) {
    while (UART_STATUS & UART_STATUS_TX_FULL); // wait until TX not full
    UART_TX_DATA = c;
}

void uart_puts(const char *s) {
    while (*s) uart_putc(*s++);
}
```

**Key Pattern 2: CSR Access from C**
```c
// Read a CSR
static inline uint32_t csr_read(int csr_num) {
    uint32_t val;
    asm volatile("csrr %0, %1" : "=r"(val) : "i"(csr_num));
    return val;
}

// Write a CSR
static inline void csr_write(int csr_num, uint32_t val) {
    asm volatile("csrw %0, %1" :: "i"(csr_num), "r"(val));
}

// CSR numbers (from privileged spec)
#define CSR_MSTATUS  0x300
#define CSR_MIE      0x304
#define CSR_MTVEC    0x305
#define CSR_MEPC     0x341
#define CSR_MCAUSE   0x342
```

**Key Pattern 3: Interrupt Setup**
```c
extern void _trap_handler(void);  // defined in assembly

void setup_interrupts(void) {
    // 1. Set trap vector
    csr_write(CSR_MTVEC, (uint32_t)&_trap_handler);

    // 2. Enable timer interrupt (bit 7 of mie = MTIE)
    uint32_t mie = csr_read(CSR_MIE);
    csr_write(CSR_MIE, mie | (1 << 7));

    // 3. Enable global interrupts (bit 3 of mstatus = MIE)
    uint32_t mstatus = csr_read(CSR_MSTATUS);
    csr_write(CSR_MSTATUS, mstatus | (1 << 3));
}
```

#### Exercise 1: Write startup.S (30 min)

Write a complete RISC-V startup assembly file:
```asm
.section .text.startup
.globl _start
_start:
    # 1. Set stack pointer
    # 2. Zero BSS section (loop from _bss_start to _bss_end)
    # 3. Call main
    # 4. Infinite loop if main returns
```

**Maps to project file**: `firmware/boot/startup.S`

#### Exercise 2: Write trap_handler.S (30 min)

```asm
.section .text.trap
.globl _trap_handler
_trap_handler:
    # 1. Save all caller-saved registers to stack (ra, t0-t6, a0-a7)
    # 2. Read mcause into a0
    # 3. Call C handler: trap_dispatch(mcause)
    # 4. Restore registers
    # 5. mret
```

**Maps to project file**: `firmware/boot/trap_handler.S`

#### Part 2: CPU↔GPU Interface — Memory Map (1.5 hours)

**Understand your SoC memory map**:
```
0x0000_0000 — 0x0000_FFFF : Instruction BRAM (64KB)
0x0001_0000 — 0x0003_FFFF : Data BRAM (192KB)
0x1000_0000 — 0x1000_000F : UART registers
0x1000_1000 — 0x1000_100F : Timer (CLINT) registers
0x1000_2000 — 0x1000_200F : GPIO registers
0x2000_0000 — 0x2000_003F : GPU command registers
0x3000_0000 — 0x3000_FFFF : GPU shared memory region
```

**Exercise 3: Write the GPU driver** (30 min)

```c
// GPU command registers
#define GPU_BASE         0x20000000
#define GPU_STATUS       REG32(GPU_BASE + 0x00)  // [0]=busy, [1]=done, [2]=error
#define GPU_CMD          REG32(GPU_BASE + 0x04)  // write 1 to launch kernel
#define GPU_KERNEL_ADDR  REG32(GPU_BASE + 0x08)  // kernel code address
#define GPU_DATA_ADDR    REG32(GPU_BASE + 0x0C)  // input data address
#define GPU_RESULT_ADDR  REG32(GPU_BASE + 0x10)  // output data address
#define GPU_NUM_THREADS  REG32(GPU_BASE + 0x14)  // number of threads to launch
#define GPU_NUM_WARPS    REG32(GPU_BASE + 0x18)  // number of warps

void gpu_launch_kernel(uint32_t kernel, uint32_t data, uint32_t result, uint32_t threads) {
    // Wait if GPU is busy
    while (GPU_STATUS & 0x1);
    // Configure
    GPU_KERNEL_ADDR = kernel;
    GPU_DATA_ADDR   = data;
    GPU_RESULT_ADDR = result;
    GPU_NUM_THREADS = threads;
    // Launch
    GPU_CMD = 1;
}

void gpu_wait(void) {
    while (!(GPU_STATUS & 0x2)); // wait for done bit
}
```

**Maps to project files**: `firmware/drivers/gpu.c`, `hardware/rtl/gpu/gpu_cmd_proc.v`

#### Checkpoint

✅ You can write MMIO driver code with `volatile`. You can write startup assembly and trap handlers. You understand the SoC memory map and how the CPU talks to the GPU through command registers.

---

### Day 10 — Integration Day: Build & Test Your First File

**Time**: ~3–4 hours

**Goal**: Prove you're ready. Write real project code, run a real testbench, see it pass.

#### Task 1: Write `regfile.v` (1 hour)

Open `hardware/rtl/cpu/core/regfile.v` and implement the register file:

**Spec**:
- 32 registers × 32 bits
- 2 read ports (rs1, rs2) — combinational (async)
- 1 write port (rd) — synchronous (posedge clk)
- `x0` hardwired to zero (reads always return 0, writes are ignored)
- Synchronous reset: all registers zeroed

**Interface** (must match testbench expectations):
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

#### Task 2: Run the Testbench (30 min)

```bash
cd hardware/sim
chmod +x run_sim.sh
./run_sim.sh cpu/tb_regfile
```

If all tests pass → **you're ready**.
If some fail → debug using the VCD waveform: `gtkwave waves/tb_regfile.vcd`

#### Task 3: Write `imm_gen.v` (1 hour)

If regfile passed, start your second file:

```verilog
module imm_gen (
    input  [31:0] instruction,
    input  [2:0]  imm_type,    // 0=I, 1=S, 2=B, 3=U, 4=J
    output reg [31:0] immediate
);
// Sign-extend immediates for each type
// Reference: RISC-V spec Fig 2.4 (immediate encoding variants)
endmodule
```

#### Task 4: Skim the Full Roadmap

Re-read the implementation roadmap (file-by-file checklist). You now understand every concept it references. Plan your next 2 weeks of coding.

#### Checkpoint

✅ `regfile.v` compiles and passes all testbench tests. You understand the Verilator workflow. You're ready to work through the roadmap file by file.

---

## Quick-Reference: What to Study When Stuck

| If you're stuck on... | Go back to... |
|---|---|
| Verilog syntax | HDLBits (P1), FPGA Prototyping book (B3) |
| Instruction encoding | Green Card (R3), Online decoder (R4) |
| Pipeline hazards | P&H Ch 4 (B1), Day 5 exercises |
| OoO / ROB / RAT | Onur Mutlu lectures (L1), H&P Ch 3 (B2), Day 6 exercise |
| Branch prediction | McFarling 1993 paper, Day 7 GShare trace |
| Cache design | P&H Ch 5 (B1), Day 7 cache math |
| GPU / SIMT / warps | Onur Mutlu GPU lecture (L1/G6), NVIDIA CUDA guide Ch 4 (G2), Day 8 |
| Vortex GPU RTL | Vortex GitHub (G3), tutorial slides (G4) |
| Bare-metal C | "RISC-V from scratch" (E1), Five EmbedDev (E2) |
| Interrupts / CSRs | RISC-V Privileged Spec (R2), Day 9 exercises |
| Verilator testbenches | Verilator guide (S1), existing `tb_alu.cpp` in project |
