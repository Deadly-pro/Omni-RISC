# Omni-RISC APU — Roadmap to the Coherent APU

*Locked model, July 2026. This is the target and the sequenced path to it. Floor first; stretch tiers are gated behind a finished floor.*

## The locked target: a coherent APU

A CPU+GPU on one die sharing **unified, consistent memory** — the thing that makes it honestly an *APU*, not "a CPU and a GPU that happen to share a die."

- **CPU**: 2× RV32IM scalar, in-order, 5-stage cores. Each core has private L1 I$ + D$.
- **CPU coherence (fine-grained)**: the 2 cores snoop each other per cache line — **snooping MSI, write-through, write-invalidate**, over a shared bus. This is the only fine-grained coherence in the design, and it is deliberately kept to 2 agents.
- **GPU**: SIMT engine, 4 warps × 4 lanes, own frontend (fetch/decode/warp scheduler), scratchpad.
- **CPU↔GPU coherence (coarse-grained)**: the GPU is **not** a fine-grained snooping peer (that doesn't scale to GPU cache/thread counts and no real APU does it). Coherence between CPU and GPU happens **at synchronization points** — CPU writes+flushes a buffer, launches a kernel (*acquire*), GPU computes+flushes (*release*), CPU invalidates+reads. The meeting point is a shared last-level ordering point / memory-side directory.
- **Consistency model**: software targets an explicit, scoped model with fences (RVWMO-style) + acquire/release at kernel boundaries. Coherence is a *contract the software fulfills*, not implicit-everywhere hardware magic. This is why compilers work.
- **Interconnect**: AXI4-Lite; GPU driven by memory-mapped command path (`gpu_cmd_proc`).

### Explicitly still OUT of scope (do not drift into these)
- OoO execution, ROB, register rename, scoreboard — archived (`ooo-scope-archive`).
- **Superscalar / dual-issue** — not in this plan. (In-order dual-issue, Cortex-A53 style, is a *possible future* stretch but is NOT locked; revisit only if the whole floor + stretch below is done with time to spare.)
- **MESI/MOESI, write-back coherence, 4+ cores, full directory** — the months-trap. We use MSI/write-through/2-core on purpose.
- **MMU / virtual memory / S-mode / booting Linux** — M-mode + a bare-metal scheduler is the OS-concepts deliverable, not a full OS.

### Scope deltas beyond plain RV32IM (introduced by the coherent model)
- **A-extension, minimal** (LR/SC, or a couple of AMOs) — needed for inter-core locks in Phase D.
- **Cache flush/invalidate operations** — custom ops or MMIO, for coarse-grained coherence in Phase F.
- **FENCE** — for the consistency model in Phases D/F.

---

## Priority tiers

| Tier | Phases | Why |
|------|--------|-----|
| **Floor (must finish)** | A (CPU→compliance), E (GPU works), G (synth+measure) | The credential: a certified RV32IM core + a working SIMT GPU, measured on real silicon. |
| **High value** | B (SoC + UART + bare-metal scheduler) | Demonstrates OS→ISA translation concretely on your own core. |
| **Stretch (makes it a true APU)** | C (caches) → D (2-core coherence) → F (coherent-APU integration) | The differentiator. Correct and bounded, but gated: F needs C+D+E. |

**Rule:** finish two complete, verified machines (certified CPU + working GPU) before starting the coherence stretch. A 40%-done coherent multicore with no GPU loses to a rock-solid scalar CPU + working GPU.

---

## Dependency graph

```
A (CPU core → compliance)
 └─> B (SoC/UART/scheduler)
 └─> C (L1 caches)
        └─> D (2-core snooping MSI coherence)
E (SIMT GPU)                                   ┐
C + D + E ────────────────────────────────────┴─> F (coherent-APU integration)
everything ───────────────────────────────────────> G (synth + measurement)
```

---

## Phase A — Single scalar core to compliance  *(FLOOR — in progress)*

Where we are: bring-up steps 1–5 done; memory subsystem (LSU + data_bram + WB aligner) done and verified; LUI/AUIPC operand-mux bug fixed; `tb_cpu_top` 24/24.

- **A1. `forwarding_net`** ← *next*. Forward EX/MEM.rd→EX and MEM/WB.rd→EX. Kills the distance-1/2 RAW hazards currently hidden by hand-inserted NOPs.
- **A2. `hazard_unit`**. Load-use stall (1 bubble; load result isn't ready until WB in this design), branch/jump flush. Then **delete the NOP crutches from the test programs** — that regression-proves forwarding + interlocks.
- **A3. M-extension in the pipeline**. Wire `multiplier` (3–4 cyc) and `divider` (iterative, stalls EX) into EX as ALU sub-ops with a stall handshake.
- **A4. CSR file + trap unit**. M-mode CSRs (`mstatus/mtvec/mepc/mcause/mtval/mie/mip`, counters). Traps: illegal instr, ecall/ebreak, misaligned. Timer interrupt (CLINT `mtime/mtimecmp`). PC joins the EX/MEM bundle for `mepc`.
- **A5. riscv-arch-test** rv32i + rv32im. Record in `docs/compliance_results.md`.
- **Done when:** one core is a certified RV32IM that passes compliance.

## Phase B — SoC + firmware proof  *(HIGH VALUE)*
- **B1.** AXI4-Lite interconnect + address decode per `memory_map.md`.
- **B2.** UART (MMIO) — `printf` from firmware.
- **B3.** Bare-metal **trap-driven scheduler** in C: two tasks, timer-interrupt context switch. *This is the OS→ISA non-negotiable, realized on your silicon.*
- **Done when:** core runs compiled firmware, prints over UART, preemptively context-switches.

## Phase C — L1 caches  *(STRETCH prerequisite)*
- **C1.** L1 I$ (direct-mapped → 2-way if time), refill FSM over AXI.
- **C2.** L1 D$ **write-through, write-allocate** — write-through chosen deliberately to simplify the Phase-D MSI protocol (no dirty-transfer race class).
- **Done when:** caches verified single-core; CPI measured before/after.

### Phase C status (Aug 2026)
Both cache MODULES are implemented and verified standalone:
- `l1_dcache.v` — 4KB 2-way write-through write-allocate, 32B lines, MMIO bypass — `tb_cache` 14/14.
- `l1_icache.v` — 4KB direct-mapped read-only, 32B lines, fetch-ahead-safe — `tb_icache` 27/27.

The caches are wired into the pipeline behind a `USE_CACHES` parameter (default 0, so `tb_cpu_top` 83/83 and compliance 53/54 run the cacheless CPU untouched). The SoC still runs `USE_CACHES=0`.

**FIXED (this commit):** the concurrent I/D-cache refill fetch-pairing bug. The fix registers the IF/ID instruction together with its pc:
- fetch_stage: on redirect/trap, captures `redirect_target`/`trap_target` directly into `if_id_pc` (not the lagging `pc`), AND feeds the same target address to the icache so its `rdata` matches.
- l1_icache: accepts `pc` = redirect_target during redirects via fetch_stage's `ic_pc` mux.
- New focused test `tb_2store.cpp` validates the 2-store+JAL pattern that previously corrupted.

**Verified:** tb_cpu_top 83/83, compliance 53/54, tb_cache 14/14, tb_icache 27/27, tb_soc_uart PASS, tb_soc_timer PASS, tb_2store PASS, tb_cpu2 PASS.

## Phase D — 2-core coherent CPU  *(STRETCH — the fine-grained coherence)*
- **D1.** Replicate the core → 2 cores on a shared bus. **✓ DONE**
  - `cpu2_top.v`: two `cpu_top` instances sharing a peripheral bus with fixed-priority arbitration
  - `l1_dcache_msi.v`: 4KB 2-way MSI snooping cache (M/S/I states, write-through, write-invalidate)
  - Snoop interfaces wired: each core snoops the other's bus transactions
  - Testbench `tb_cpu2.cpp` builds and runs

- **D2.** Shared bus + snooping: each D$ snoops the other's transactions. **✓ DONE** (in D1)
- **D3.** **Snooping MSI** per D$ line (write-through, write-invalidate). Handle transient states (snoop-hit-during-miss, invalidate/eviction races, same-cycle conflicts) — this, not the 3-state FSM, is the real work. **✓ DONE**
- **D4.** Minimal **A-extension** (LR/SC) for a working spinlock. **✓ DONE**
  - LR.W/SC.W decode → exec → MSI D-cache → WB path verified single-core (`tb_lrsc` 5/5)
  - Shared-memory dual-core: both L1 D-caches refill/write-through a **single physical data_bram** via a fixed-priority arbiter (`mem_stage SHARED_MEM`, `dual_core_top`)
  - MSI snooping watches the other core's **dcache memory transactions** (not pbus)
  - `tb_dual_spin` (both cores LR/SC a shared lock, increment a counter to 1000): **no lost updates, PASS**
- **D5.** **Litmus verification**: message-passing, store-buffering, shared-counter-with-atomics. Coherence bugs are heisenbugs — interleaving stress is mandatory. **✓ DONE**
  - Fixed two real coherence bugs D5 exposed:
    - **Snoop starvation**: a core's dcache only captured snoops in IDLE/CHECK and CPU requests beat snoops, so a busy poller never saw the other core's writes. Fix: latch snoops in EVERY state and process them with priority in IDLE.
    - **Request-pulse loss**: the shared-memory arbiter could defer a core's refill read, missing the 1-cycle request pulse → deadlock. Fix: LEVEL handshake (hold `mem_read_req` until ack).
  - Message-passing litmus (data must be visible when flag is): **200/200 PASS**
  - Store-buffering litmus (forbidden both-read-0): **200/200 PASS, no forbidden outcome**
- **Done when:** 2 cores share coherent memory, a spinlock is correct, litmus tests pass. **✓ DONE** — Phase D complete.

## Phase E — SIMT GPU  *(FLOOR)*
- **E1.** Lane datapath: `gpu_regfile`, `gpu_alu`, `gpu_scratchpad`, `gpu_lsu`.
- **E2.** `exec_lane` × 4 = one warp, with per-lane predication/masking.
- **E3.** `warp_scheduler` (4 warps) + `gpu_fetch` + `gpu_decode`.
- **E4.** `gpu_top` + `gpu_cmd_proc` (MMIO command interface).
- **E5.** Kernels: `vector_add` first, then `relu`/`conv2d`/`matmul`; verify against a scalar reference.
- **Done when:** GPU runs kernels standalone with correct results. **✓ DONE (Aug 2026)** — 7/7 GPU TBs green (`tb_gpu_top_kernels` 66/66: vector_add, relu, 2x2 matmul, 3-tap conv2d across 4 warps); `scripts/gpu_asm.py` assembles the 16-bit SIMT ISA to `$readmemh` hex.

## Phase F — Coherent-APU integration  *(STRETCH — the capstone)*
- **F1.** Unified shared memory + shared coherence/ordering point (shared LLC or memory-side directory).
- *Partial (Aug 2026):* the shared-memory **window** is in — the CPU reads/writes warp0's scratchpad over `HOST_WIN`/`HOST_DATA` (`+0x18`/`+0x1C`, pbus slave @0x40002000), launches via `drivers/gpu.c`, polls `STATUS` (`+0x20`) to zero (release), reads results back through the same window (acquire). Verified end-to-end in `tb_soc_gpu`: CPU writes A/B → vector_add kernel computes C → per-lane readback `C = A + B` → firmware PASS. Still missing for full coherence: a unified kernel memory (kernel image is still flashed into IMEM at synth time), and CPU↔GPU flush/invalidate beyond the window.
- **F2.** Cache **flush/invalidate** ops for CPU D$ and GPU caches.
- **F3.** **Acquire/release handshake** at kernel launch/completion in `gpu_cmd_proc` + driver.
- **F4.** End-to-end demo: CPU allocates a buffer in unified memory, writes+flushes, launches a GPU kernel via MMIO, GPU computes+flushes, CPU invalidates+reads — **coherent, with no manual copies**.
- **F5.** Define + document the memory consistency model (fences, scopes).
- **Done when:** CPU and GPU cooperate on shared data at sync-point granularity, coherence maintained, no explicit copies.

## Phase G — Synthesis + measurement  *(FLOOR)*
- **Flow is in (Aug 2026):** `make synth` = Vivado 2026.1 (`/tools/2026.1/Vivado`) batch on `soc_top` (xc7a100tcsg324-1, 50 MHz). Reports in `hardware/vivado/reports/`.

- **BRAM fit DONE (Aug 2026):** `data_bram` read is now registered (`always @(posedge clk) rdata <= mem[addr[17:2]]`), which lets Vivado map the 256KB arrays into BRAM instead of distributed RAM. Supporting pipeline-timing changes: `mem_stage` inserts a 1-cycle `mem_hold` per data-window load (`dbram_hold` toggle) and toggles `dc_mem_ack` for dcache refills; `dual_core_top` toggles per-core read acks; `decode_stage` gives `hold` priority over `flush` (a branch/jump frozen in EX by a hold must not have its bundle cleared by its own redirect before EX/MEM captures it — a real bug that showed up as `tb_soc_gpu` printing "GPU(4444…", fixed by reordering the ID/EX update priority).

- **Results (post-fix, real firmware image staged):** synthesis 6,378 LUTs (10%) incl. **LUT-as-Memory 0 (was 32,768 = 172% overflow)**, 10,720 FF (8.5%), **64 RAMB36E1 (47.8%)**, 18 DSP; synth setup WNS +3.10ns. Implementation (place+route, no board — timing is the fit gate): **6,270 LUTs (9.9%), 64.5 BRAM tiles (47.8%), 10,720 FF, 18 DSP; setup WNS +1.305ns at 50 MHz → Fmax ≈ 53.5 MHz**. Critical path: `u_cpu/u_mem/mem_wb_rdata[31]` → `u_cpu/u_exec/ex_mem_alu_result[29]` (WB→EX forwarding into the ALU operand mux). Hold +0.117ns, PW +9.5ns, both clean.
  - Remaining: `impl.dcp` exists; bitstream deliberately off (no board target). Next lever for Fmax is pipelining/retiming the register-file→ALU forwarding mux if a higher clock is ever needed.
- CPI on real programs (CoreMark/Dhrystone). GPU kernel speedup vs scalar.
- **Done when:** numbers are recorded and the writeup is done.