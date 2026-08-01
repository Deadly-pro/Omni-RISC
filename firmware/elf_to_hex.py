#!/usr/bin/env python3
"""Convert a flat binary (objcopy -O binary of the firmware ELF) into a
$readmemh word hex.

The firmware is linked as one contiguous image from address 0x00000000, so the
flat word offset equals the BRAM index (addr >> 2), which is exactly how
instr_bram/data_bram are indexed. Output: one 32-bit word per line.
"""
import sys


def main() -> None:
    src, dst = sys.argv[1], sys.argv[2]
    with open(src, "rb") as f:
        data = f.read()
    if len(data) % 4:
        data += b"\x00" * (4 - len(data) % 4)
    with open(dst, "w") as f:
        for i in range(0, len(data), 4):
            w = int.from_bytes(data[i : i + 4], "little")
            f.write("%08X\n" % w)


if __name__ == "__main__":
    main()
