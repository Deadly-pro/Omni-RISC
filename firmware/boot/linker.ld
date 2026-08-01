/* Omni-RISC APU — Linker Script for RV32IM Bare-Metal Firmware
 *
 * Single flat 64KB region at 0x00000000. The CPU has separate instruction and
 * data BRAMs, both indexed by the low address bits, so code and data may share
 * the flat address space; keeping them contiguous from 0x0 makes the ELF load
 * into both memories from one flat hex image.
 */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM (rwx) : ORIGIN = 0x00000000, LENGTH = 64K
}

SECTIONS
{
    /* Code */
    .text : {
        *(.text.startup)    /* _start and early boot */
        *(.text.trap)       /* trap handler */
        *(.text*)           /* all other code */
    } > RAM

    /* Read-only data */
    .rodata : {
        *(.rodata*)
        *(.srodata*)
    } > RAM

    /* Initialized data */
    .data : {
        _data_start = .;
        *(.data*)
        *(.sdata*)
        _data_end = .;
    } > RAM

    /* Uninitialized data (zeroed by boot code) */
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        _bss_end = .;
    } > RAM

    /* Stack at top of memory (grows down; first access is below 64K) */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
