#!/usr/bin/env python3
"""
test_conv_numerical.py — Generate numerical reference data for INT8 convolution.

This script:
  1. Generates a random INT8 input tensor and a random INT8 kernel.
  2. Computes the expected conv2d output using PyTorch.
  3. Quantises the output to INT8 (right-shift + clamp to [-128, 127]).
  4. Saves input, kernel, and expected output as .hex files
     (one value per line, in two-character hex).
  5. Verifies the saved files by re-reading them and comparing against
     the PyTorch reference.

The .hex files are suitable for loading into simulation via $readmemh.

Usage:
    python3 tests/test_conv_numerical.py
    python3 tests/test_conv_numerical.py --input-size 16 --kernel-size 5
    python3 tests/test_conv_numerical.py --output-dir /path/to/output
"""

import argparse
import os
import sys
import struct

import numpy as np

try:
    import torch
    import torch.nn.functional as F
except ImportError:
    print("ERROR: PyTorch is required. Install with: pip install torch")
    sys.exit(1)


# ============================================================================
# Hex file I/O helpers
# ============================================================================

def int8_to_hex(val: int) -> str:
    """
    Convert a signed INT8 value to a 2-character hex string.

    The value is interpreted as an unsigned byte (two's complement).
    For example:  -1 → 'ff',  127 → '7f',  0 → '00'.
    """
    return format(val & 0xFF, '02x')


def hex_to_int8(h: str) -> int:
    """
    Convert a 2-character hex string back to a signed INT8 value.

    For example:  'ff' → -1,  '7f' → 127,  '00' → 0.
    """
    unsigned = int(h, 16)
    if unsigned >= 128:
        return unsigned - 256
    return unsigned


def save_hex_file(filepath: str, values: np.ndarray) -> None:
    """
    Save a 1-D or 2-D INT8 numpy array as a .hex file.

    For 2-D arrays the data is flattened row-major. Each line contains
    one hex byte (2 characters).
    """
    flat = values.flatten().astype(np.int8)
    with open(filepath, 'w') as f:
        for v in flat:
            f.write(int8_to_hex(int(v)) + '\n')
    print(f"  Saved {len(flat)} values to {filepath}")


def load_hex_file(filepath: str) -> np.ndarray:
    """
    Load a .hex file (one 2-char hex value per line) into a 1-D INT8 array.
    """
    values = []
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                values.append(hex_to_int8(line))
    return np.array(values, dtype=np.int8)


# ============================================================================
# Convolution reference computation
# ============================================================================

def compute_conv2d_int8(
    input_data: np.ndarray,
    kernel_data: np.ndarray,
    shift: int = 7
) -> np.ndarray:
    """
    Compute 2-D convolution of INT8 input and kernel using PyTorch,
    then quantise the result back to INT8.

    Args:
        input_data:  2-D INT8 numpy array, shape (H, W).
        kernel_data: 2-D INT8 numpy array, shape (kH, kW).
        shift:       Right-shift applied to the raw 32-bit accumulator to
                     fit the result into INT8. Default 7 (divide by 128).

    Returns:
        2-D INT8 numpy array of shape (H - kH + 1, W - kW + 1).

    The quantisation is:
        out_int8 = clamp( raw_int32 >> shift, -128, 127 )
    """
    # Convert to float tensors for PyTorch conv2d.
    # We keep the actual integer values (no scaling) so the arithmetic
    # matches what the hardware would do.
    inp_tensor = torch.tensor(
        input_data.astype(np.float32)
    ).unsqueeze(0).unsqueeze(0)  # shape: (1, 1, H, W)

    ker_tensor = torch.tensor(
        kernel_data.astype(np.float32)
    ).unsqueeze(0).unsqueeze(0)  # shape: (1, 1, kH, kW)

    # Valid (no padding) convolution
    raw_output = F.conv2d(inp_tensor, ker_tensor, padding=0)

    # Convert back to numpy int32 for quantisation
    raw_np = raw_output.squeeze().numpy().astype(np.int32)

    # Quantise: arithmetic right-shift + clamp to INT8 range
    quantised = np.clip(raw_np >> shift, -128, 127).astype(np.int8)

    return quantised


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate INT8 conv2d reference data for simulation."
    )
    parser.add_argument(
        '--input-size', type=int, default=8,
        help='Height and width of the square input tensor (default: 8).'
    )
    parser.add_argument(
        '--kernel-size', type=int, default=3,
        help='Height and width of the square convolution kernel (default: 3).'
    )
    parser.add_argument(
        '--shift', type=int, default=7,
        help='Right-shift for quantising conv output to INT8 (default: 7).'
    )
    parser.add_argument(
        '--seed', type=int, default=42,
        help='Random seed for reproducibility (default: 42).'
    )
    parser.add_argument(
        '--output-dir', type=str,
        default=None,
        help='Directory to write .hex files. Default: hardware/sim/reference_data/'
    )
    args = parser.parse_args()

    # Resolve output directory
    if args.output_dir is None:
        # Default: relative to project root
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(script_dir)
        output_dir = os.path.join(project_root, 'hardware', 'sim', 'reference_data')
    else:
        output_dir = args.output_dir

    os.makedirs(output_dir, exist_ok=True)

    # -----------------------------------------------------------------------
    # Generate random INT8 data
    # -----------------------------------------------------------------------
    np.random.seed(args.seed)

    H = W = args.input_size
    kH = kW = args.kernel_size

    # INT8 range: -128..127
    input_data = np.random.randint(-128, 128, size=(H, W), dtype=np.int8)
    kernel_data = np.random.randint(-128, 128, size=(kH, kW), dtype=np.int8)

    print(f"[conv] Input:  {H}×{W} INT8")
    print(f"[conv] Kernel: {kH}×{kW} INT8")
    print(f"[conv] Shift:  {args.shift}")
    print(f"[conv] Seed:   {args.seed}")
    print()

    # -----------------------------------------------------------------------
    # Compute reference convolution
    # -----------------------------------------------------------------------
    output_data = compute_conv2d_int8(input_data, kernel_data, shift=args.shift)
    oH, oW = output_data.shape
    print(f"[conv] Output: {oH}×{oW} INT8")
    print()

    # -----------------------------------------------------------------------
    # Save to .hex files
    # -----------------------------------------------------------------------
    input_hex  = os.path.join(output_dir, 'conv_input.hex')
    kernel_hex = os.path.join(output_dir, 'conv_kernel.hex')
    output_hex = os.path.join(output_dir, 'conv_expected_output.hex')

    print("[conv] Saving hex files:")
    save_hex_file(input_hex,  input_data)
    save_hex_file(kernel_hex, kernel_data)
    save_hex_file(output_hex, output_data)
    print()

    # -----------------------------------------------------------------------
    # Verify by re-reading the hex files
    # -----------------------------------------------------------------------
    print("[conv] Verification (re-read hex files):")

    loaded_input  = load_hex_file(input_hex)
    loaded_kernel = load_hex_file(kernel_hex)
    loaded_output = load_hex_file(output_hex)

    # Compare flat arrays
    input_ok  = np.array_equal(loaded_input,  input_data.flatten())
    kernel_ok = np.array_equal(loaded_kernel, kernel_data.flatten())
    output_ok = np.array_equal(loaded_output, output_data.flatten())

    print(f"  Input  roundtrip: {'PASS' if input_ok else 'FAIL'}")
    print(f"  Kernel roundtrip: {'PASS' if kernel_ok else 'FAIL'}")
    print(f"  Output roundtrip: {'PASS' if output_ok else 'FAIL'}")

    if not (input_ok and kernel_ok and output_ok):
        print("\nERROR: Hex file verification failed!")
        sys.exit(1)

    # Extra check: recompute conv from loaded data and compare
    loaded_input_2d  = loaded_input.reshape(H, W)
    loaded_kernel_2d = loaded_kernel.reshape(kH, kW)
    recomputed = compute_conv2d_int8(loaded_input_2d, loaded_kernel_2d,
                                     shift=args.shift)
    recompute_ok = np.array_equal(recomputed.flatten(), loaded_output)
    print(f"  Recomputation:    {'PASS' if recompute_ok else 'FAIL'}")

    if not recompute_ok:
        print("\nERROR: Recomputed output does not match saved output!")
        sys.exit(1)

    print("\n[conv] All checks passed. Hex files ready for simulation.")


if __name__ == '__main__':
    main()
