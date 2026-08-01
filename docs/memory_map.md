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
