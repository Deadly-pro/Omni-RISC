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

/* One flat loadable image. PHDRS is load-bearing, not cosmetic: without it ld
 * synthesises a PT_PHDR segment for the ELF headers, and binutils 2.47
 * (CachyOS) rejects the result with "PHDR segment not covered by LOAD segment";
 * the older toolchain only warned. Declaring the segment explicitly keeps the
 * image identical — objcopy -O binary still starts at _start, which is vaddr 0.
 */
PHDRS
{
    image PT_LOAD;
}

SECTIONS
{
    /* Code */
    .text : {
        *(.text.startup)    /* _start and early boot */
        *(.text.trap)       /* trap handler */
        *(.text*)           /* all other code */
    } > RAM :image

    /* Read-only data */
    .rodata : {
        *(.rodata*)
        *(.srodata*)
    } > RAM :image

    /* Initialized data */
    .data : {
        _data_start = .;
        *(.data*)
        *(.sdata*)
        _data_end = .;
    } > RAM :image

    /* Uninitialized data (zeroed by boot code) */
    .bss (NOLOAD) : {
        _bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        _bss_end = .;
    } > RAM :image

    /* Stack at top of memory (grows down; first access is below 64K) */
    _stack_top = ORIGIN(RAM) + LENGTH(RAM);
}
