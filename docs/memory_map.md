# Omni-RISC APU — memory map

The SoC has a flat 32-bit address space. The CPU drives a simple peripheral bus
(`pbus`), which is active for addresses at or above `0x0004_0000`; everything
below that window is the internal data BRAM. `soc_top` decodes the pbus to the
peripheral slaves.

## Map

| Address range                | Slave            | Notes |
|------------------------------|------------------|-------|
| `0x0000_0000` – `0x0000_FFFF` | IMEM (code)      | `instr_bram` inside `cpu_top` (fetch path). Firmware linked here. |
| `0x0001_0000` – `0x0003_FFFF` | DMEM (data)      | `data_bram` inside `cpu_top`. Firmware data/stack live here (the 256KB window wraps the BRAM index). |
| `0x0200_0000` – `0x0200_FFFF` | CLINT / timer    | `mtime` free-runs; `mtip` asserts when `mtime >= mtimecmp`. |
| `0x4000_0000` – `0x4000_0FFF` | UART             | TX-only 115200-8N1. |
| `0x4000_1000` – `0x4000_1FFF` | GPIO             | 8-bit output. |
| `0x4000_2000` – `0x4000_2FFF` | GPU              | SIMT engine: cmd regs + kernel result readback. |

### CLINT / timer (`hardware/rtl/soc/timer.v`)

| Offset   | Register     | Access | Notes |
|----------|--------------|--------|-------|
| `+0x4000` | `mtimecmp[31:0]` | RW | compare value low word |
| `+0x4004` | `mtimecmp[63:32]`| RW | compare value high word |
| `+0xBFF8` | `mtime[31:0]`    | RW | free-running counter low |
| `+0xBFFC` | `mtime[63:32]`   | RW | free-running counter high |

`mtip` (mip.MTIP) is read-only in the CPU CSR; the firmware clears it by
writing `mtimecmp` forward.

### UART (`hardware/rtl/soc/uart.v`)

| Offset | Register | Access | Notes |
|--------|----------|--------|-------|
| `+0x00` | TX       | W      | write a byte to transmit (8N1 @115200) |
| `+0x04` | STATUS   | R      | bit0 = TX busy (1 = transmitting) |

Baud fixed: 115200 at a 50 MHz system clock (÷434).

### GPIO (`hardware/rtl/soc/gpio.v`)

| Offset | Register | Access | Notes |
|--------|----------|--------|-------|
| `+0x00` | GPIO out | RW     | 8-bit output; reads return the current value |

### GPU (`hardware/rtl/gpu/gpu_top.v`, pbus slave)

| Offset | Register     | Access | Notes |
|--------|--------------|--------|-------|
| `+0x00` | warp_pc[0]   | RW     | start address of warp 0's kernel |
| `+0x04` | warp_pc[1]   | RW     | warp 1 (unused by single-kernel demo) |
| `+0x08` | warp_pc[2]   | RW     | warp 2 |
| `+0x0C` | warp_pc[3]   | RW     | warp 3 |
| `+0x10` | LAUNCH       | W      | bit31 = go, bit[1:0] = warp id |
| `+0x14` | RESULT       | R      | warp0 scratchpad word 0 (host readback) |
| `+0x18` | HOST_WIN     | RW     | shared-memory window: `{bank[1:0], word[7:0]}` |
| `+0x1C` | HOST_DATA    | RW     | window store (write) / load (read) |
| `+0x20` | STATUS       | R      | low 4 bits = active_warps bitmap |

The readback decode uses `pbus_addr[5:0]` so `+0x20` (STATUS) does not alias
`+0x00`. Shared-memory window access (`HOST_WIN`/`HOST_DATA`) reads/writes
warp0's 4-bank scratchpad: `bank` selects the SIMT sub-lane, `word` the word
address within that bank.

The CPU dispatches a kernel by writing `warp_pc[0]`, then `LAUNCH`, then polling
STATUS until `active_warps == 0`, then reading results. Kernel inputs are
written into the scratchpad via HOST_WIN/HOST_DATA **before** LAUNCH (acquire
for the GPU: stores land before the pbus write completes); polling STATUS to
zero is the GPU's release. The kernel is flashed into GPU imem at synthesis
time (`.IMEM_FILE` → `$readmemh`); pushing kernel hex over pbus is future
unified-memory work.

## Firmware agreement

## Firmware agreement

`firmware/drivers/uart.c`, `timer.c`, `gpio.c` and `boot/boot.c` use exactly
these addresses. The firmware links as one contiguous image at `0x0000_0000`
(see `firmware/boot/linker.ld`), which loads into both instruction and data
BRAMs from a single flat `$readmemh` hex.

## Upgrades

- **AXI4-Lite**: the pbus is a simple request/response decode; a full AXI4-Lite
  interconnect can replace the decoder in `soc_top` later without touching the
  CPU (it already exposes the master interface).
- UART RX, extra timers/PLIC, and caches are future work.
