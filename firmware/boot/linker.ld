/* Omni-RISC APU — Linker Script for RV32IM Bare-Metal Firmware */

OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    /* Instruction BRAM: 64KB starting at 0x00000000 */
    IMEM (rx)  : ORIGIN = 0x00000000, LENGTH = 64K

    /* Data BRAM: 192KB starting at 0x00010000 */
    DMEM (rwx) : ORIGIN = 0x00010000, LENGTH = 192K
}

SECTIONS
{
    /* Code goes into instruction memory */
    .text : {
        *(.text.startup)    /* _start and early boot */
        *(.text.trap)       /* trap handler */
        *(.text*)           /* all other code */
    } > IMEM

    /* Read-only data */
    .rodata : {
        *(.rodata*)
        *(.srodata*)
    } > DMEM

    /* Initialized data */
    .data : {
        _data_start = .;
        *(.data*)
        *(.sdata*)
        _data_end = .;
    } > DMEM

    /* Uninitialized data (zeroed by boot code) */
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        _bss_end = .;
    } > DMEM

    /* Stack at top of data memory */
    _stack_top = ORIGIN(DMEM) + LENGTH(DMEM);
}
