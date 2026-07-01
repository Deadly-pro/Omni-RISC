#!/usr/bin/env python3
"""
npy_to_hex.py — Convert NumPy .npy files to simulation-ready .hex files.

Reads a .npy file containing a NumPy array and writes a text file with one
hex-encoded value per line, suitable for Verilog $readmemh.

Supported data types:
    int8   → 2 hex characters per value   (e.g., 'ff')
    int16  → 4 hex characters per value   (e.g., 'ffff')
    int32  → 8 hex characters per value   (e.g., 'ffffffff')

Multi-dimensional arrays are flattened in row-major (C) order.

Usage:
    python3 scripts/npy_to_hex.py --input data.npy --output data.hex --dtype int8
    python3 scripts/npy_to_hex.py -i weights.npy -o weights.hex -d int32
"""

import argparse
import os
import sys

import numpy as np


# ============================================================================
# Conversion parameters per dtype
# ============================================================================

# Maps dtype name → (numpy dtype, number of hex chars, unsigned mask)
DTYPE_CONFIG = {
    'int8':  (np.int8,  2, 0xFF),
    'int16': (np.int16, 4, 0xFFFF),
    'int32': (np.int32, 8, 0xFFFFFFFF),
}


# ============================================================================
# Conversion logic
# ============================================================================

def convert_npy_to_hex(
    input_path: str,
    output_path: str,
    dtype_name: str,
    verbose: bool = True
) -> int:
    """
    Convert a .npy file to a .hex text file.

    Args:
        input_path:  Path to the input .npy file.
        output_path: Path to the output .hex file.
        dtype_name:  One of 'int8', 'int16', 'int32'.
        verbose:     Print progress information.

    Returns:
        Number of values written.

    Raises:
        FileNotFoundError: If input file does not exist.
        ValueError:        If dtype_name is not supported.
    """
    # Validate dtype
    if dtype_name not in DTYPE_CONFIG:
        raise ValueError(
            f"Unsupported dtype '{dtype_name}'. "
            f"Supported: {list(DTYPE_CONFIG.keys())}"
        )

    np_dtype, hex_chars, mask = DTYPE_CONFIG[dtype_name]

    # Load the numpy array
    if not os.path.isfile(input_path):
        raise FileNotFoundError(f"Input file not found: {input_path}")

    data = np.load(input_path)

    if verbose:
        print(f"[npy2hex] Input file:  {input_path}")
        print(f"[npy2hex] Array shape: {data.shape}")
        print(f"[npy2hex] Array dtype: {data.dtype}")
        print(f"[npy2hex] Target type: {dtype_name} ({hex_chars} hex chars)")

    # Cast to the target dtype.
    # If the source dtype is floating point, truncate to integer.
    if np.issubdtype(data.dtype, np.floating):
        if verbose:
            print(f"[npy2hex] WARNING: Converting float → {dtype_name} "
                  f"(truncation, not rounding)")
        data = data.astype(np_dtype)
    elif data.dtype != np_dtype:
        if verbose:
            print(f"[npy2hex] Casting {data.dtype} → {np_dtype}")
        data = data.astype(np_dtype)

    # Flatten to 1-D (row-major order)
    flat = data.flatten()
    count = len(flat)

    # Write hex file
    fmt = f'{{:0{hex_chars}x}}'
    with open(output_path, 'w') as f:
        for val in flat:
            # Apply unsigned mask for two's complement representation
            hex_val = fmt.format(int(val) & mask)
            f.write(hex_val + '\n')

    if verbose:
        print(f"[npy2hex] Output file: {output_path}")
        print(f"[npy2hex] Wrote {count} values")
        # Show first few and last few values as a sanity check
        preview_n = min(4, count)
        print(f"[npy2hex] First {preview_n}: "
              f"{[fmt.format(int(v) & mask) for v in flat[:preview_n]]}")
        if count > preview_n:
            print(f"[npy2hex] Last  {preview_n}: "
                  f"{[fmt.format(int(v) & mask) for v in flat[-preview_n:]]}")

    return count


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Convert a NumPy .npy file to a $readmemh-compatible "
                    ".hex text file."
    )
    parser.add_argument(
        '-i', '--input', required=True,
        help='Path to input .npy file.'
    )
    parser.add_argument(
        '-o', '--output', required=True,
        help='Path to output .hex file.'
    )
    parser.add_argument(
        '-d', '--dtype', required=True,
        choices=['int8', 'int16', 'int32'],
        help='Data type for hex encoding: int8 (2 chars), '
             'int16 (4 chars), int32 (8 chars).'
    )
    parser.add_argument(
        '-q', '--quiet', action='store_true',
        help='Suppress informational output.'
    )
    args = parser.parse_args()

    try:
        count = convert_npy_to_hex(
            input_path=args.input,
            output_path=args.output,
            dtype_name=args.dtype,
            verbose=not args.quiet
        )
        if not args.quiet:
            print(f"\n[npy2hex] Done. {count} values converted successfully.")
    except FileNotFoundError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)
    except Exception as e:
        print(f"ERROR: Unexpected error: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == '__main__':
    main()
