# Firmware track — bare-metal C, gate by gate

**Status (2026-10-13): F0–F8 are shipped and green.** All eight phases are implemented in
the tree, each with its gate passing — see the verified-gate list under the phase table.
The deal stood for F1: the gate app was given with weak stubs, and the per-case diagnostics
it prints are what the implementation was grown against.

Goal: grow the APU's firmware layer by hand, one feature at a time. Same deal as the
Verilog phase: **the tests come first, the implementation is yours.** You are given a
gate that fails against the tree as it stands; you make it pass.

---

## 1. The deal

Every phase ships with three things:

| Given | Contents |
|---|---|
| **The gate** | a script under `tests/` plus its vectors/reference. Exit 0 = pass. It fails today. |
| **The interface** | the header (signatures, types) you code against, plus the register tables in `docs/memory_map.md`. |
| **Hints** | a short list per phase. No code. |

**You write:** the `.c` file(s) by hand — that is the whole point. Roughly 100–400 lines
per phase, freestanding C, no libc, no `malloc` until F5 gives you one.

Rules of engagement:

1. **Never edit a gate, its vectors, its reference model, or the harness TB** to make
   something pass. If you think a gate is wrong, write it up in the phase's Notes and
   stop — we fix the gate, not the test.
2. Vectors are **generated with a random seed that the gate prints**, so hardcoding
   answers cannot pass. Read the seed, reproduce failures, don't memorise outputs.
3. One commit per phase, gate green. Body of the commit: the gate's summary line
   (e.g. `[LIB] 200/200 PASS`) plus one line on anything that surprised you.
4. "Unspecified" is part of the exercise. If the spec doesn't say what `%d` does with
   `INT_MIN`, decide, document the decision, and make the gate agree with the C
   standard.
5. Don't skip phases. F1/F2 are the tools every later phase debugs with.

## 2. Where the firmware stands today

Deliberately thin — that is why this track exists:

```
firmware/boot/       startup.S (sp, .bss clear), trap_handler.S (save regs, call C), boot.c
firmware/drivers/    uart.c 37 lines · timer.c 37 · gpu.c 32 · gpio.c 13   (+ .h, all tiny)
firmware/rtos/       omni_libc.c 65 lines = memcpy, memset, strcmp, strlen, abort + 2 weak hooks
firmware/apps/       shell.c 424 lines (FreeRTOS console, command table), plus per-TB apps
```

What you can already reach from C:

| Block | Base | Notes |
|---|---|---|
| UART | `0x4000_0000` | 115200-8N1 (÷434 @ 50 MHz). `+0x00` TX (W), `+0x04` STATUS (bit0 TX busy, bit1 RX ready, bit2 **sticky overrun**), `+0x08` RXDATA (read pops). **16-byte RX FIFO. No interrupt line** (verified: `uart.v` has no IRQ output). |
| CLINT | `0x0200_0000` | `mtime` RW (`+0xBFF8`), `mtimecmp` RW (`+0x4000`) → `mtip`. `msip` → software interrupt. |
| GPIO | `0x4000_1000` | 8-bit out, reads back. |
| GPU | `0x4000_2000` | `WARP_PC0..3`, `LAUNCH` (+0x10, bit31 = go), `RESULT` (+0x14), `HOST_WIN`/`HOST_DATA` (scratchpad window), `STATUS` (+0x20, low nibble = active_warps). Kernel is `$readmemh`'d into GPU IMEM **at synthesis time**. |

Interrupt sources wired into the CPU **today**: `mtip` (timer) and `msip` (software) —
the GPU raises `msip` on kernel completion (`soc_top` → `gpu_msip(gpu_done)`). There is
no external (MEIP) line and no UART IRQ, so genuinely interrupt-driven UART RX needs an
RTL change (that is F9, optional).

Address space: IMEM `0x0000_0000`–`0x0000_FFFF` (64K, code); DMEM `0x0001_0000`–`0x0003_FFFF`
(192K, data/bss/heap, `_stack_top = 0x0004_0000`); pbus lives at `≥ 0x0004_0000`.

## 3. How a gate runs (F0 — shipped 2026-10-12)

`tb_soc_shell` is reusable for the shell only (it waits for the `[SHELL] ready` banner, and
`run_sim.sh` hardcodes `APP=shell` for it), so the track has a generic rig instead:

- **`hardware/sim/soc/tb_soc_console.cpp`** — boots *any* image staged as
  `obj_dir_tb_soc_console/program.hex`, decodes UART TX at real framing (434 clk/bit) onto
  **stdout**, feeds stdin onto `uart_rx` the same way, and ends the simulation on: the end
  marker in the TX stream (`TB_END_MARK`, default `[TEST] DONE`); or stdin EOF plus
  `TB_QUIET` cycles of TX silence; or `TB_MAX` cycles, which exits 1. Pacing is
  `TB_PACE=burst` (default — back-to-back frames, a genuine FIFO burst) or `=line` (wait for
  the reply to drain). It prints a `[TB]` heartbeat every `TB_HEARTBEAT` cycles, so a hung
  run is diagnosable instead of silent.
- **`tests/run_app.sh <app> [session-file]`** — builds `APP=<app>` (build chatter goes to
  **stderr**, so stdout is purely the firmware's UART output and a gate can byte-diff it),
  stages the hex, runs the TB, and returns its status: 0 = marker or EOF-and-quiet, 1 = cap
  hit, 2 = build failed. Env passthrough: `APP_DEFS`, `TB_*`. `TB_GPU_IMAGE=none` leaves GPU
  IMEM stubbed, which F8 will need.
- **`tests/check.sh`** — assertion helpers (`check_init`, `check_has`, `check_count`,
  `check_absent`, `check_pass`) in the same `ok` / `FAIL` style as `tests/test_shell.sh`.

Firmware convention the gates rely on:

```
[TEST] <family> <pass>/<total> PASS        one line per family when green
[TEST] <family>[<idx>] FAIL want=.. got=..  a detail line per failing case
[TEST] SUMMARY <pass>/<total>
[TEST] DONE                                 ends the simulation (the TB watches for it)
```

**Transcript size is simulation time.** A byte costs ~4,340 cycles at real 115200 baud, so
gates print compactly: a green run is a dozen lines, and the gate app caps failure detail at
3 lines per family. If a gate ever feels slow, look at how much it prints.

## 4. Phases at a glance

| Phase | Feature | You write | Depends on | Effort |
|---|---|---|---|---|
| F1 | string/memory library you own | `lib/omni_string.c` | F0 | 1–2 |
| F2 | a printf worth debugging with | `lib/omni_printf.c` | F1 | 3–4 |
| F3 | event-driven UART: ring buffer, overrun policy, timeouts | `drivers/uart_rx.c` | F1,F2 | 2–3 |
| F4 | framed host protocol with CRC + retransmit | `net/frame.c` + `tests/peer.py` peer | F2,F3 | 4–6 |
| F5 | a real allocator (first-fit + coalescing) | `lib/omni_alloc.c` | F1 | 4–6 |
| F6 | fault diagnostics / panic handler | `boot/fault.c` | F2 | 2–3 |
| F7 | monotonic time, delays, software timers | `drivers/tick.c` | F2 | 3–4 |
| F8 | **capstone:** load a GPU kernel at runtime | `drivers/gpu_loader.c` (+ RTL) | F4,F7 | 8–12 |
| F9 | *optional:* a peripheral in RTL + its driver | RTL + `drivers/spi.c` | F3,F8 | 6–10 |

**Gates, as they actually run (all green 2026-10-13, `riscv64-linux-gnu-gcc` 16.2.1):**

```
./tests/test_libc.sh        ./tests/test_convert.sh    # F1, 130/130 and 260/260 checks
./tests/test_printf.sh                                  # F2, 93/93 + 2 byte-identical streams
./tests/test_uart.sh                                    # F3, burst 4/4, line+timeout 5/5
./tests/test_framing.sh                                 # F4, 10/10 vs the receiver simulator
./tests/test_alloc.sh                                   # F5, 12/12
./tests/test_fault.sh                                   # F6, 4 fault kinds
./tests/test_timers.sh                                  # F7, 13/13
./tests/test_gpu_loader.sh                              # F8, 5/5 twice (kernel pushed / not)
```

F9 stays optional and unwritten. A handful of these gates disagree with the phase text below
about names and output shapes — the list above is what exists: F3's gate is `test_uart.sh`
(not `test_uart_burst.sh`), and F5's app prints `[TEST] SUMMARY 7/7` with the gate reporting
`allocator: ALL PASS (12 checks)` rather than an `[ALLOC]` line.

Effort is in focused evenings (2–3 h). Order is a suggestion: F1→F2→F3→F4 is a spine;
F5/F6/F7 can be interleaved; F8 last.

---

## F1 — String/memory library you own

**Goal.** Replace the four-function `omni_libc.c` with a library you understand, and
learn what "freestanding" really costs.

**You write.** `firmware/lib/omni_string.c`. The interface is given, with every contract
written out: `firmware/include/omni/string.h` — `memcpy`, `memmove`, `memset`, `memcmp`,
`strlen`, `strcpy`, `strncpy`, `strcmp`, `strncmp`, `strchr`, `strrchr`, `strtoul`, `strtol`,
`itoa`, `utoa_hex`, plus the per-call `OMNI_ERANGE` status behind `omni_conv_errno()`. Any
`firmware/lib/*.c` is compiled and linked into the non-RTOS apps automatically — no Makefile
edit needed.

**Gates.** Two of them, because the numeric half is its own fight:

```bash
./tests/test_libc.sh                 # memory + string, 11 functions
./tests/test_convert.sh              # same app built -DWITH_CONV: + strtoul/strtol/itoa/utoa_hex
./tests/test_libc.sh --seed 12345    # pin the seed a failure is reproducible with
```

Each prints `SEED=`, `TOTAL_MEM=` and `TOTAL_CONV=`, then `ok` / `FAIL` per assertion, and
exits 0 only when every vector passes.

**Vectors.** `tests/gen_libc_vectors.py` draws them from a seed (printed, and echoed by the
app so a stale table is caught) into two gitignored headers. Memory/string expectations are
checked in-firmware against a byte-wise local reference; the `strtoul`/`strtol` expectations
are computed in Python, and `--emit-refcheck` writes a host C program that cross-checks that
reference against the host libc:

```bash
python3 tests/gen_libc_vectors.py --emit-refcheck /tmp/refcheck.c
gcc -Ifirmware/apps -o /tmp/refcheck /tmp/refcheck.c && /tmp/refcheck
```

**Before you write anything the gates still run.** The app carries weak, deliberately wrong
stubs for every function it calls, so you get per-case diagnostics from your first line of
code instead of `undefined reference to memcpy`. Your strong definitions override them.

**Concepts.** `size_t`/pointer arithmetic; why `memcpy` may not overlap but `memmove` must;
sign extension and unsigned wraparound; `endptr` on "no conversion" (it is the ORIGINAL
pointer, not one past the sign); a `-` for the unsigned function negating modulo 2**32 rather
than overflowing.

**Done when.** Both gates green, zero warnings with `-Wall`, and you can explain what your
`memmove` does when `dst` and `src` overlap by one byte.

**Hints.** Byte loop first — correctness before speed; alignment is a correctness issue, not
just a speed one. **There is no libgcc in this link**: `__udivdi3`/`__muldi3` and friends do
not exist, so keep the conversion accumulator 32-bit and saturate as soon as it would pass
`ULONG_MAX` — while still consuming the remaining digits, because `endptr` must land past
them. Decide and document whatever the header leaves open.

---

## F2 — A printf worth debugging with

**Goal.** Kill the hand-rolled duplication and get a real formatting engine. There are
**six** private formatters today, each with its own digit loop: `put_dec` in `shell.c`,
`rtos_metrics.c`, `rtos_q.c`, `rtos_sem.c`; `put_hex` in `rtos.c`; `put_hex32` in
`shell.c`.

**You write.** `firmware/lib/omni_printf.c`: `vsnprintf`, `snprintf`, `printf`,
`vprintf` (sink = UART). Support `%d %i %u %x %X %c %s %p %%`, flags `-`, `0`, `+`,
space, `#`, decimal width and `.precision`, and the C99 truncation contract (return the
length that *would* have been written; always NUL-terminate when `size > 0`).

**You write.** `firmware/lib/omni_printf.c`. The interface is given, with the contract
written out: `firmware/include/omni/stdio.h` — `vsnprintf`, `snprintf`, `printf`,
`vprintf`, the supported subset (`%d %i %u %x %X %c %s %p %%`; flags `-` `0` `+` space
`#`; decimal width; `.precision`) and the C99 truncation rules: at most `n` bytes
including the NUL, always terminate when `n > 0`, write nothing when `n == 0`, and
return the *would-be* length, so truncation is invisible in the return value.

**Gate.** `./tests/test_printf.sh [--seed N]` — one gate, six checks:

- `tests/gen_printf_vectors.py` draws the table (hand-written corner cases plus a seeded
  random batch) and emits a host program that runs the **same** cases through the **host
  C library's** `snprintf`. That program writes the expectations — `ret`, whether
  anything outside the written region was touched, and the output bytes — and the
  firmware app checks itself against them, so a failure names the case and the byte.
- both streaming entry points are then diffed byte-for-byte: `printf` and `vprintf` run
  the same formats between markers and each block must equal the C library's output,
  which also proves the two paths agree.

Cases include `INT_MIN`, `%u` of `4294967295`, `%x` of `0`, `%#x` of `0` (no prefix),
`%#010x`, `%+d`, `% d`, `%.3d`, `%.0d` of `0` (empty), `%08.3d` (the `0` flag is ignored
once a precision is given), `%-8.3d`, `%10.3s`, `%p`, and truncation at `n = 0/1/2/4/5`.

The random batch is the point: expectations change with the seed, so memorising outputs
cannot pass. As in F1, the app carries weak deliberately-wrong stubs, so the gate runs
and reports per-case diagnostics before you have written anything.

**Concepts.** `va_list`/`va_arg` and default argument promotion; why `%f` is not here
(float formatting is a phase of its own, deliberately out of scope); buffer-bounds
discipline; the difference between "wrote" and "would have written".

**Done when.** Gate green, and the demo path actually uses it: wire `omni_printf.c` into
the RTOS link (dropping the duplicate formatters) and keep `./tests/test_shell.sh` at
17/17.

**Hints.** Parse flags/width/precision into fields first, render last. Keep counting
characters after the buffer is full — that *is* the truncation contract. The `0` flag is
ignored once a precision is given, `-` beats `0`, and `+` beats space. `%p` takes width
only: flags or precision with `%p` are undefined, so the gate never uses them. No libgcc
here, so keep the digit loops 32-bit. `%s` of NULL is UB — decide and document.

---

## F3 — Event-driven UART: ring buffer, overrun policy, timeouts

**Goal.** Stop polling `uart_getchar()` in a loop. Own the receive path.

**You write.** `firmware/drivers/uart_rx.c`: a software ring buffer (size-of-your-choosing,
power of two), `uart_rx_pump()` that drains the 16-byte RTL FIFO into the ring,
`uart_rx_available()`, `uart_rx_read(buf, n)`, `uart_rx_getline(buf, n, timeout_ticks)`,
plus counters: `rx_bytes`, `rx_dropped`, `rx_overruns`, `rx_errors`.

**Gate.** `./tests/test_uart.sh`, in two parts:

1. **Accounting.** The harness sends 1 byte, then a 64-byte burst, then a 1024-byte
   burst with no pacing (the RTL FIFO *will* overflow — it is 16 bytes deep). Assert the
   firmware reports the exact bytes it consumed, that the sticky overrun bit was observed
   and cleared, that counters add up (`consumed + dropped == sent`), and that the bytes it
   did receive are in order. **Dropping is allowed here; silently losing bytes is not.**
2. **No-loss bulk transfer.** With a simple host handshake you design (XON/XOFF,
   a sentinel, or a length-prefixed "wait" reply — your call, documented), a 1024-byte
   transfer must complete with `rx_dropped == 0`. The gate asserts the content hash,
   not just the count.

Plus a **timeout** case: no input → `uart_rx_getline` returns the timeout code within
±10% of the requested tick count (measured against `mtime`).

**Concepts.** Producer/consumer with a single writer (an ISR would need atomics — here
the pump is called from one context, so say so in a comment); buffer-full policy;
timeouts vs. blocking; why "count what you dropped" beats "hope".

**Done when.** Gate green and the shell's console task uses your `getline` instead of
its own byte loop.

**Hints.** `uart_status()` bit2 is sticky — read STATUS once, log it, then read RXDATA
to clear. Pump before you block, or you will starve the FIFO.

---

## F4 — Framed host protocol (the debug link you will actually use)

**Goal.** A byte-exact protocol between the simulated APU and a Python peer on the host,
surviving garbage, truncation and bit flips.

**You write.** `firmware/net/frame.c`: frame = `0x7E` flag, byte-stuffed body
(`len`, `seq`, `cmd`, `payload`, `crc16-CCITT`), ACK/NAK replies, retransmit on NAK with
a bounded retry count, a small command dispatch table (echo, read-mem, write-mem,
reset-stats) and an idle-timeout that discards a partial frame. **Plus**
`tests/peer.py` — the host side: build/send frames, parse replies, compute the CRC.
(Writing the peer is half the exercise: it forces you to specify the wire format.)

**Gate.** `./tests/test_framing.sh` (drives `tests/peer.py` against the sim, seeded):
valid frames ACK and reassemble; a single bit-flipped frame NAKs and never yields a
payload; a truncated frame followed by a fresh one recovers; 4 KB of random binary
garbage produces no crash and no false frame; 5000 frames back-to-back lose nothing;
`seq` wraparound behaves (state your policy).

**Concepts.** Frame synchronisation and resync after corruption; CRC choice and
implementing it bitwise then by table; sequence numbers and duplicate detection;
"never trust the length field"; designing a test peer so it can *disagree* with you.

**Hints.** Get the CRC right against a known vector (`"123456789"` → `0x29B1` for
CRC-16/CCITT-FALSE) before anything else. State-machine-per-byte is easier to debug than
buffer-then-parse. Keep a `frame_errors` counter and print it in the frame stats command.

---

## F5 — Memory management: a real allocator

**Goal.** Own the heap. This is where pointer bugs are born, so build the invariant
checker with it.

**You write.** `firmware/lib/omni_alloc.c` plus the linker-script arena you need:
`malloc`, `free`, `calloc`, `realloc`, and `heap_stats()` (largest free block, free
bytes, fragmentation ratio, block count). First-fit or best-fit free list, 8-byte
alignment, split on allocate, **coalesce with both neighbours on free**, double-free
detection.

**Gate.** `./tests/test_alloc.sh` → `allocator: ALL PASS (12 checks)`, with the app's own
`[TEST] SUMMARY 7/7` and `leaks=0` after the soak:

- alignment of every returned pointer across sizes 1…4096;
- coalescing: allocate A/B/C, free all three, one allocation of `A+B+C+24` must succeed;
- `realloc`: grow in place when possible, move-with-copy when not, `realloc(p, 0)`
  policy stated and enforced;
- a **randomized soak** (~10k ops, seeded) interleaving alloc/free/realloc with data
  written and verified after every operation;
- after the soak: an arena walk asserting the free list is sorted, non-overlapping, and
  sums to the arena size (`leaks=0`), plus a fragmentation number for the record.

**Concepts.** Free-list headers, alignment vs. padding, fragmentation, invariants you
can *check* rather than reason about, why `calloc` must multiply without overflow.

**Done when.** Gate green, and you can state the allocator's worst-case behaviour and
what you deliberately did *not* implement (no `mmap`, no threads, no per-size arenas).

**Hints.** Keep a magic word and a size in the header; validate both on `free` — that's
how you catch double frees. Write `heap_check()` early and call it after every
operation in the soak; the first failing op number is your bug.

---

## F6 — Fault diagnostics: a panic handler worth having

**Goal.** The current `trap_handler.S` calls `timer_int_handler(mcause)` for *every*
trap, so an illegal instruction or bad load silently becomes "advance the timer".

**You write.** `firmware/boot/fault.c`: decode `mcause` into
`illegal instruction / load fault / store fault / misaligned / ecall(U) / ebreak /
timer / software`, print `mcause`, `mepc`, `mtval`, `mscratch`, `mstatus` and all 31
registers with `printf` (F2), maintain a fault counter, and implement `ecall` from
M-mode as a **deliberate semihost exit** (this is how your tests will end cleanly).
Wire the trap route so timer/MSI traps still go to the normal handler.

**Gate.** `./tests/test_fault.sh`: a `fault <kind>` shell command / test app triggers
each class (illegal opcode via `.word`, load from an unmapped pbus address, misaligned
load, `ecall`, `ebreak`) and the gate asserts the decoded cause name and the `mepc`
value in the dump; the `ecall` case must exit the sim with the exit marker.

**Concepts.** M-mode trap entry (`mcause`/`mepc`/`mtval` semantics, which bits are
valid when), why the stack is still usable in a handler, telling faults and interrupts
apart by `mcause[31]`.

**Hints.** Print before you fix anything — a dump that never appears is worse than a
wrong one. `mtval` is 0 for some causes; state your interpretation. Keep the dump
machine-greppable: one `key=value` per register.

---

## F7 — Monotonic time, delays, software timers

**Goal.** Every later phase needs "wait exactly N ms" and "call this every N ms".

**You write.** `firmware/drivers/tick.c`: `ticks_ms()` / `ticks_us()` from the 64-bit
`mtime` (rollover-safe comparisons using unsigned subtraction), `delay_ms`/`delay_us`
(spin — this core has no `wfi`), a software timer list (`timer_add(period, one_shot,
cb)`, `timer_cancel`, at least 8 timers) driven from the MTI trap, per-timer stats
(expiries, late ticks), and drift measurement.

**Gate.** `./tests/test_timers.sh`: ordering (three timers with the same period fire in
registration order), period accuracy within ±1 tick measured against `mtime`, a
callback that overruns its period does not lose or duplicate the next expiry, cancel
works (and cancel-from-inside-a-callback works), and the 32-bit-boundary case is
exercised (force `mtime` near `0xFFFF_FFFF` and confirm no jump).

**Concepts.** Interrupt-vs-main-loop split, callback reentrancy, `volatile` for
variables touched by the trap, why 64-bit compare needs care, drift accumulation.

**Hints.** Do the arithmetic with `uint32_t` deltas (`now - last`) before touching the
64-bit compare. Keep the ISR to "set a flag / bump a counter" if you find yourself
printing from it.

---

## F8 — Capstone: load a GPU kernel at runtime

**Goal.** Close the project's own biggest gap. Today the kernel is `$readmemh`'d into
GPU IMEM at synthesis (`memory_map.md`: "pushing kernel hex over pbus is future
unified-memory work"). Make the CPU *load* a kernel into the GPU and run it.

**You write.** RTL (a small addition) + `firmware/drivers/gpu_loader.c`: a pbus-reachable
write port into GPU IMEM (e.g. `GPU_IMEM_ADDR`/`GPU_IMEM_DATA`, or an auto-incrementing
write data register with an address register), then a C loader that receives kernel words
over F4, writes them, writes `WARP_PC0`, launches, **waits on the GPU completion
interrupt** (`gpu_msip` → your F6/F7 handling) instead of polling `STATUS`, and verifies
the checksum against `tests/test_gpu_kernels.py`'s model. Add a `load <name>` shell
command so the demo shows it.

**Gate.** `./tests/test_gpu_loader.sh`: the TB starts with GPU IMEM **stubbed/empty**;
the host pushes a kernel at run time over the frame protocol; the firmware loads,
launches and reports a checksum that must equal the Python model's; run it for
`vector_add`, `relu` and `matmul`. A second case pushes a deliberately truncated kernel
and asserts the firmware *refuses* to launch rather than executing garbage.

**Concepts.** Memory-mapped device programming from the CPU side; ordering/visibility
(the acquire before `LAUNCH`, the release at completion) — the actual coherence story
this project is about; the difference between "the RTL works" and "the firmware drives it
correctly"; adding an RTL port without breaking `make synth` timing.

**Hints.** Bring the RTL port up with `tb_gpu_top_*` first (a pure RTL TB), *then* write
the C loader — otherwise you will debug two layers at once. Keep the write port's
handshake level-based (the doc's own lesson from the pbus arbiter). If timing regresses
in `make synth`, say so in the commit and record the number.

---

## F9 — Optional stretch: a peripheral in RTL, driven from C

Pick one, it is the same exercise each time: **(a)** an interrupt line for the UART
(`uart.v` → a `mip` bit) and a genuinely ISR-driven RX ring buffer, replacing F3's
pumped one; **(b)** an SPI or I²C master with a loopback slave TB, plus
`drivers/spi.c` and a device (an external ADC/sensor model in the TB); **(c)** a
performance-counter block (`mcycle`/`minstret` are already in the CPU CSRs — start by
*using* them from C for a cycle-accurate profiler of the pipeline, then decide whether
the RTL needs anything).

Same contract: RTL + driver + a gate that fails today, passes when done, and does not
require editing the gate to get there.

---

## 5. When you get stuck

- **Bisect the layer first.** Pure C bug → add `printf` cases, run the gate by hand.
  Firmware/hardware boundary → `./hardware/sim/run_sim.sh soc/tb_soc_console --wave`
  (the F0 harness) and look at the pbus/UART signals in GTKWave. RTL bug → a
  `tb_gpu_top_*`-style TB before touching the driver.
- **`make dump`** (`firmware/Makefile`) gives you a disassembly of `APP=<app>`; useful
  when the compiler outsmarts you.
- **One failing case at a time.** Every gate prints per-case `FAIL` lines with actual
  values; resist fixing two at once.
- **Record the surprise.** The repo's own docs are built from postmortems
  (`docs/rtos_bringup.md`, `docs/plan_rtos_to_terminal.md`). Your phase notes are the
  same genre — and they are what makes the next phase cheap.

## 6. Notes per phase (the traps that actually cost time)

```
F1: Two halves, and both bite. (a) The library: `size_t` needs <stddef.h> before use, and
    strncmp must apply its bound BEFORE dereferencing or it reads past the buffer. (b) The
    conversions: no libgcc in a -nostdlib link, so 64-bit divide/multiply are undefined
    (__udivdi3/__muldi3) - saturate in 32-bit instead of reaching for unsigned long long.
    omni_conv_errno() is per-call state, not global errno.
F2: glibc is the oracle, so get the awkward cases from it rather than from intuition:
    "%.0d" of 0 is ret=0 with a lone NUL (not "0"); "%-8.3d" of -7 is "-007    "; %p is a
    bare 0x hex with no 0-padding. Build the gate app with -fno-builtin (the Makefile does
    it for apps/%_test.o) or gcc folds snprintf(buf, n, "literal", ...) into an inline copy
    and the test exercises the compiler instead of your formatter.
F3: A UART test that only ever feeds bytes proves nothing about loss. Stall deliberately so
    the 16-byte hardware FIFO overruns, then assert consumed + reconstructed loss == sent.
    Read STATUS before RXDATA - reading RXDATA pops.
F4: The receiver state machine is where the bugs live (escaping, resync, duplicate seq), so
    its reference is a byte-level simulator, not hand-counted expectations. Anchor test
    patterns ("seq=2 cmd=1") or "seq=2" will happily match "seq=255".
F5: Give the allocator a heap_check() that proves the arena is coherent (sizes in bounds, the
    walk ends exactly at the end, no two free neighbours) and call it after EVERY operation
    of a randomised soak - then a failure names the first bad op instead of "something is
    wrong". Real bug class worth knowing: the soak ran ~6M cycles with no UART output, so the
    harness's 3M-cycle quiet window decided the test had finished and cut it off mid-run.
F6: Route interrupts and exceptions separately: before this, every trap went to the timer
    handler, so an illegal instruction silently advanced the clock. Which traps exist is a
    property of the RTL: there is no MMU/PMP here (access faults cannot happen) and the core
    does NOT raise misaligned-access exceptions - it just performs the access. The gate says
    so explicitly rather than demanding a dump the hardware will never produce.
F7: Armed mtimecmp is not an enabled interrupt: without mie.MTIE and mstatus.MIE every
    callback silently never runs. mtime's low word rolls over every ~86 s, so elapsed time is
    always an unsigned difference - and computing microseconds as mtime()/50 is a 64-bit
    divide (F1's __udivdi3 trap again); accumulate a base in the ISR and divide 32 bits.
F8: The GPU could only run a kernel flashed at synthesis time. It now takes words over
    pbus (IMEM_ADDR 0x4000_2024, IMEM_DATA 0x4000_2028 installs and advances), and waits on
    the completion interrupt (GPU done -> CLINT msip -> mcause 0x80000003) instead of polling
    STATUS. The load port must stay RAM-inferable: write-only, no reset. Two traps: Verilator
    exits on warnings here, so widening the command decode from 5 to 6 bits meant every case
    item had to be widened too ("%Error: Exiting due to 7 warning(s)"); and a kernel's input
    layout is in its own header - vector_add.S says A @ 0, B @ 16, C @ 32, and reading C from
    word 0 returns A, which looks exactly like "the kernel did nothing".
```

## 7. What you will be able to say afterwards

"Built the firmware layer of my RISC-V APU by hand: freestanding string/format library,
event-driven UART with overrun accounting and a CRC-framed host protocol, a coalescing
allocator with an invariant checker, M-mode fault diagnostics, a software timer service,
and a runtime GPU kernel loader driven over pbus with the project's acquire/release
coherence discipline."
