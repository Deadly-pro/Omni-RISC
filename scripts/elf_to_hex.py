#!/usr/bin/env python3
"""
elf_to_hex.py — Convert a RISC-V ELF binary to a $readmemh-compatible hex file.

Reads a RISC-V ELF file, extracts the .text and .data sections, and
produces a hex file with one 32-bit word per line (8 hex characters).

The output is suitable for loading into Verilog simulation memories
using $readmemh.

Addresses are mapped relative to a configurable base address:
    physical_addr = section_vaddr - base_addr

Gaps between sections (or within sparse sections) are filled with
a configurable fill value (default: 0x00000000 = NOP for RISC-V).

Requirements:
    pip install pyelftools

Usage:
    python3 scripts/elf_to_hex.py --input program.elf --output program.hex
    python3 scripts/elf_to_hex.py -i test.elf -o test.hex --base-addr 0x80000000
"""

import argparse
import os
import sys
import struct

try:
    from elftools.elf.elffile import ELFFile
    from elftools.elf.sections import Section
except ImportError:
    print("ERROR: pyelftools is required. Install with: pip install pyelftools",
          file=sys.stderr)
    sys.exit(1)


# ============================================================================
# ELF extraction
# ============================================================================

def extract_sections(elf_path: str, section_names: list) -> list:
    """
    Extract named sections from an ELF file.

    Args:
        elf_path:      Path to the ELF file.
        section_names: List of section names to extract (e.g., ['.text', '.data']).

    Returns:
        List of tuples: (section_name, virtual_address, bytes_data)
        sorted by virtual address. Sections not found in the ELF are skipped.

    Raises:
        FileNotFoundError: If ELF file does not exist.
    """
    if not os.path.isfile(elf_path):
        raise FileNotFoundError(f"ELF file not found: {elf_path}")

    sections = []

    with open(elf_path, 'rb') as f:
        try:
            elf = ELFFile(f)
        except Exception as e:
            raise ValueError(f"Not a valid ELF file: {elf_path} ({e})")

        # Verify this is a RISC-V ELF
        machine = elf.header.e_machine
        # EM_RISCV = 243
        if machine not in ('EM_RISCV', 243):
            print(f"WARNING: ELF machine type is {machine}, expected EM_RISCV (243)")

        # Verify 32-bit
        elfclass = elf.elfclass
        if elfclass != 32:
            print(f"WARNING: ELF class is {elfclass}-bit, expected 32-bit")

        for name in section_names:
            section = elf.get_section_by_name(name)
            if section is None:
                print(f"  Section '{name}' not found in ELF, skipping.")
                continue

            vaddr = section['sh_addr']
            data  = section.data()
            size  = len(data)

            if size == 0:
                print(f"  Section '{name}' is empty, skipping.")
                continue

            print(f"  Section '{name}': vaddr=0x{vaddr:08x}, size={size} bytes")
            sections.append((name, vaddr, data))

    # Sort by virtual address
    sections.sort(key=lambda s: s[1])
    return sections


def sections_to_memory_image(
    sections: list,
    base_addr: int,
    fill_value: int = 0x00
) -> bytearray:
    """
    Merge extracted ELF sections into a flat memory image.

    Args:
        sections:   List of (name, vaddr, data) tuples, sorted by vaddr.
        base_addr:  Base address of the memory region.
        fill_value: Byte value used to fill gaps (default 0x00).

    Returns:
        A bytearray representing the flat memory image starting at base_addr.
        The length covers from base_addr to the end of the last section.

    Raises:
        ValueError: If a section starts below base_addr.
    """
    if not sections:
        return bytearray()

    # Determine total size
    first_offset = sections[0][1] - base_addr
    if first_offset < 0:
        raise ValueError(
            f"Section '{sections[0][0]}' at 0x{sections[0][1]:08x} is below "
            f"base address 0x{base_addr:08x}"
        )

    last_section = sections[-1]
    total_size = (last_section[1] - base_addr) + len(last_section[2])

    # Pad to 4-byte alignment
    if total_size % 4 != 0:
        total_size += 4 - (total_size % 4)

    # Create the image filled with fill_value
    image = bytearray([fill_value] * total_size)

    # Copy each section's data into the image
    for name, vaddr, data in sections:
        offset = vaddr - base_addr
        end = offset + len(data)
        if end > total_size:
            print(f"  WARNING: Section '{name}' extends beyond image boundary, "
                  f"truncating.")
            data = data[:total_size - offset]
            end = total_size
        image[offset:end] = data

    return image


# ============================================================================
# Hex file generation
# ============================================================================

def memory_image_to_hex(image: bytearray, output_path: str) -> int:
    """
    Write a flat memory image as a $readmemh-compatible hex file.

    Each line contains one 32-bit word (8 hex characters).
    Words are read from the image in little-endian byte order
    (matching RISC-V's native byte order).

    Args:
        image:       The flat memory image (bytearray).
        output_path: Path to the output .hex file.

    Returns:
        Number of 32-bit words written.
    """
    num_words = len(image) // 4
    with open(output_path, 'w') as f:
        for i in range(num_words):
            # Read 4 bytes in little-endian order and pack as a 32-bit word
            word = struct.unpack_from('<I', image, i * 4)[0]
            f.write(f'{word:08x}\n')
    return num_words


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convert a RISC-V ELF binary to a $readmemh-compatible "
                    "hex file."
    )
    parser.add_argument(
        '-i', '--input', required=True,
        help='Path to the input RISC-V ELF file.'
    )
    parser.add_argument(
        '-o', '--output', required=True,
        help='Path to the output .hex file.'
    )
    parser.add_argument(
        '--base-addr', type=str, default='0x00000000',
        help='Base address of the memory region (hex or decimal). '
             'Default: 0x00000000. Section addresses in the ELF are mapped '
             'relative to this base.'
    )
    parser.add_argument(
        '--sections', type=str, nargs='+',
        default=['.text', '.data', '.rodata', '.sdata', '.bss'],
        help='Section names to extract from the ELF. '
             'Default: .text .data .rodata .sdata .bss'
    )
    parser.add_argument(
        '-q', '--quiet', action='store_true',
        help='Suppress informational output.'
    )
    args = parser.parse_args()

    # Parse base address (support hex like 0x80000000 or decimal)
    try:
        base_addr = int(args.base_addr, 0)
    except ValueError:
        print(f"ERROR: Invalid base address: {args.base_addr}", file=sys.stderr)
        sys.exit(1)

    verbose = not args.quiet

    if verbose:
        print(f"[elf2hex] Input ELF:    {args.input}")
        print(f"[elf2hex] Output HEX:   {args.output}")
        print(f"[elf2hex] Base address:  0x{base_addr:08x}")
        print(f"[elf2hex] Sections:      {args.sections}")
        print()

    # Step 1: Extract sections from ELF
    try:
        if verbose:
            print("[elf2hex] Extracting sections from ELF:")
        sections = extract_sections(args.input, args.sections)
    except FileNotFoundError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    if not sections:
        print("WARNING: No sections extracted. Output file will be empty.",
              file=sys.stderr)
        # Write an empty file
        with open(args.output, 'w') as f:
            pass
        sys.exit(0)

    # Step 2: Merge into a flat memory image
    try:
        if verbose:
            print(f"\n[elf2hex] Building flat memory image (base=0x{base_addr:08x}):")
        image = sections_to_memory_image(sections, base_addr)
        if verbose:
            print(f"  Image size: {len(image)} bytes ({len(image) // 4} words)")
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    # Step 3: Write hex file
    if verbose:
        print(f"\n[elf2hex] Writing hex file:")
    num_words = memory_image_to_hex(image, args.output)
    if verbose:
        print(f"  Wrote {num_words} words to {args.output}")

        # Preview first and last few words
        preview_n = min(4, num_words)
        print(f"\n[elf2hex] First {preview_n} words:")
        for i in range(preview_n):
            word = struct.unpack_from('<I', image, i * 4)[0]
            print(f"  [{i:4d}] 0x{word:08x}")
        if num_words > preview_n:
            print(f"  ...")
            print(f"[elf2hex] Last {preview_n} words:")
            for i in range(num_words - preview_n, num_words):
                word = struct.unpack_from('<I', image, i * 4)[0]
                print(f"  [{i:4d}] 0x{word:08x}")

    if verbose:
        print(f"\n[elf2hex] Done. {num_words} words converted successfully.")


if __name__ == '__main__':
    main()
