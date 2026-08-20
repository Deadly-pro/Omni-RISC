/* Omni-RISC APU — FreeRTOS linker script.
 *
 * The CPU has separate instruction and data BRAMs, both indexed by the low
 * address bits (instr_bram uses pc[17:2], data_bram uses addr[17:2], both
 * 256KB). To give the RTOS room for the ISR stack + task stacks + heap, code
 * lives in the low (instruction) region and data/heap in the high (data)
 * region. Both are one flat $readmemh image; the BRAMs ignore the high bits
 * that don't apply to them.
 *
 * Region split (matches docs/memory_map.md):
 *   0x0000_0000 – 0x0000_FFFF   IMEM (code)      — 64KB
 *   0x0001_0000 – 0x0003_FFFF   DMEM (data/heap) — 192KB of the 256KB window
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    IMEM (rx)  : ORIGIN = 0x00000000, LENGTH = 64K
    DMEM (rw)  : ORIGIN = 0x00010000, LENGTH = 192K
}

SECTIONS
{
    /* Code + read-only data in the instruction region */
    .text : {
        *(.text.startup)
        *(.text.trap)
        *(.text*)
    } > IMEM

    .rodata : {
        *(.rodata*)
        *(.srodata*)
    } > IMEM

    /* Initialized data + uninitialized data + heap in the data region */
    .data : {
        _data_start = .;
        *(.data*)
        *(.sdata*)
        _data_end = .;
    } > DMEM

    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        _bss_end = .;
    } > DMEM

    /* Heap up to the top of the data window; stack grows down from top */
    _heap_start = _bss_end;
    _heap_end = ORIGIN(DMEM) + LENGTH(DMEM) - 64K;   /* leave room for stack(s) */
    _stack_top = ORIGIN(DMEM) + LENGTH(DMEM);
}
