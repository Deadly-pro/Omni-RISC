/* Omni-RISC DUT linker script for riscv-arch-test.
 *
 * Memory model: the CPU has TWO separate 4KB BRAMs.
 *   - Instructions fetched from instr_bram via pc[11:2]  -> code at 0x0
 *   - Loads/stores hit data_bram via addr[11:2]          -> data aliases in
 *     any 4KB window; we keep data in 0x0000_0000+ so it maps to data_bram
 *     by its low 12 bits.
 * All code (.text.init + .text.rvtest + .text.rvmodel) is placed contiguously
 * at 0x0 so it fits the single I-mem window. Data follows after; the test's
 * data+signature footprint stays under 4KB so D-mem aliases stay unique.
 */
/* Base at 1MB so qemu-user can mmap the ELF (kernel mmap_min_addr blocks
 * sub-64KB mappings). The Omni-RISC BRAMs decode addr[17:2] (256KB window),
 * so a 1MB base wraps cleanly into them. DUT and reference share this base,
 * keeping PC-relative signature values (auipc, jal link) identical. */
RAM_ORIGIN = 0x00100000;
RAM_LENGTH = 0x80000;
TEST_BASE = 0x00100000;
NUM_HARTS = 1;
STACK_SIZE = 0x1000;

OUTPUT_ARCH("riscv")
ENTRY(rvtest_entry_point)

MEMORY
{
  ram (rwx) : ORIGIN = RAM_ORIGIN, LENGTH = RAM_LENGTH
}

PROVIDE(__stack_size = STACK_SIZE);
PROVIDE(__num_harts = NUM_HARTS);

SECTIONS
{
  ASSERT(TEST_BASE >= ORIGIN(ram) && TEST_BASE < ORIGIN(ram) + LENGTH(ram), "TEST_BASE is outside RAM")

  .text.init   TEST_BASE : { *(.text.init) } > ram
  .text.rvtest . : { *(.text.rvtest) *(.text.rvtest.*) } > ram
  .text.rvmodel . : { *(.text.rvmodel) *(.text.rvmodel.*) *(.text) *(.text.*) } > ram

  . = ALIGN(16);
  .rodata . : { *(.rodata) *(.rodata.*) *(.srodata) *(.srodata.*) } > ram

  . = ALIGN(16);
  .data . : { *(.data) *(.data.*) *(.sdata) *(.sdata.*) } > ram

  . = ALIGN(16);
  .bss . : {
    __bss_start = .;
    *(.sbss) *(.sbss.*) *(.bss) *(.bss.*) *(COMMON)
    . = ALIGN(16);
    __bss_end = .;
  } > ram

  . = ALIGN(16);
  __stack_bottom = .;
  . += __stack_size * __num_harts;
  __stack_top = .;

  . = ALIGN(16);
  _end = .;

  ASSERT(_end <= ORIGIN(ram) + LENGTH(ram), "ACT ELF exceeds RAM; enlarge RAM_LENGTH")
}
