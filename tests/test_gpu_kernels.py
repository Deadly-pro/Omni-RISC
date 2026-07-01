#!/usr/bin/env python3
"""
test_gpu_kernels.py — Generate reference data for GPU kernel verification.

Creates .hex test-vector files for three GPU kernels:

  1. Vector addition:       C[i] = A[i] + B[i],          length 16
  2. Matrix multiplication: C = A × B,                    4×4 INT32
  3. Element-wise ReLU:     Y[i] = max(0, X[i]),          length 16

Each data set is saved as one or more .hex files (one value per line).
INT32 values are stored as 8-character hex strings.

These files are intended to be loaded by RTL testbenches via $readmemh
or consumed by Verilator C++ testbenches.

Usage:
    python3 tests/test_gpu_kernels.py
    python3 tests/test_gpu_kernels.py --output-dir /some/path
"""

import argparse
import os
import sys
import numpy as np


# ============================================================================
# Hex file helpers
# ============================================================================

def int32_to_hex(val: int) -> str:
    """
    Convert a signed INT32 to an 8-character hex string (two's complement).

    Examples:
        0          → '00000000'
        1          → '00000001'
        -1         → 'ffffffff'
        2147483647 → '7fffffff'
    """
    return format(val & 0xFFFFFFFF, '08x')


def hex_to_int32(h: str) -> int:
    """
    Convert an 8-character hex string to a signed INT32.
    """
    unsigned = int(h, 16)
    if unsigned >= 0x80000000:
        return unsigned - 0x100000000
    return unsigned


def save_int32_hex(filepath: str, values: np.ndarray) -> None:
    """
    Save a numpy array of INT32 values as a .hex file.
    2-D arrays are flattened row-major.
    """
    flat = values.flatten().astype(np.int32)
    with open(filepath, 'w') as f:
        for v in flat:
            f.write(int32_to_hex(int(v)) + '\n')
    print(f"  Saved {len(flat)} INT32 values → {filepath}")


def load_int32_hex(filepath: str) -> np.ndarray:
    """
    Load a .hex file of INT32 values (one 8-char hex per line).
    """
    values = []
    with open(filepath, 'r') as f:
        for line in f:
            line = line.strip()
            if line:
                values.append(hex_to_int32(line))
    return np.array(values, dtype=np.int32)


# ============================================================================
# Kernel 1: Vector Addition  C[i] = A[i] + B[i]
# ============================================================================

def generate_vector_add(output_dir: str, rng: np.random.RandomState,
                        length: int = 16) -> bool:
    """
    Generate test vectors for a GPU vector-addition kernel.

    Args:
        output_dir: Directory to write .hex files.
        rng:        NumPy random state for reproducibility.
        length:     Vector length (default 16).

    Returns:
        True if verification passes.
    """
    print("\n[vecadd] Generating vector addition test data")
    print(f"  Length: {length}")

    # Random INT32 values in a moderate range to avoid overflow
    A = rng.randint(-10000, 10001, size=length).astype(np.int32)
    B = rng.randint(-10000, 10001, size=length).astype(np.int32)
    C = (A.astype(np.int64) + B.astype(np.int64)).astype(np.int32)

    save_int32_hex(os.path.join(output_dir, 'vecadd_a.hex'), A)
    save_int32_hex(os.path.join(output_dir, 'vecadd_b.hex'), B)
    save_int32_hex(os.path.join(output_dir, 'vecadd_c_expected.hex'), C)

    # Verify
    A2 = load_int32_hex(os.path.join(output_dir, 'vecadd_a.hex'))
    B2 = load_int32_hex(os.path.join(output_dir, 'vecadd_b.hex'))
    C2 = load_int32_hex(os.path.join(output_dir, 'vecadd_c_expected.hex'))

    ok = (np.array_equal(A, A2) and np.array_equal(B, B2) and
          np.array_equal(C, C2) and np.array_equal(A2 + B2, C2))
    print(f"  Verification: {'PASS' if ok else 'FAIL'}")
    return ok


# ============================================================================
# Kernel 2: Matrix Multiplication  C = A × B
# ============================================================================

def generate_matmul(output_dir: str, rng: np.random.RandomState,
                    dim: int = 4) -> bool:
    """
    Generate test data for a GPU matrix-multiply kernel (square matrices).

    Args:
        output_dir: Directory to write .hex files.
        rng:        NumPy random state.
        dim:        Matrix dimension (default 4 → 4×4).

    Returns:
        True if verification passes.
    """
    print(f"\n[matmul] Generating {dim}×{dim} matrix multiplication test data")

    # Keep values small to avoid INT32 overflow in accumulation
    # With dim=4 and values in [-50, 50], max accumulator is 4*50*50 = 10000
    A = rng.randint(-50, 51, size=(dim, dim)).astype(np.int32)
    B = rng.randint(-50, 51, size=(dim, dim)).astype(np.int32)
    # Use int64 for intermediate to avoid overflow, then cast to int32
    C = (A.astype(np.int64) @ B.astype(np.int64)).astype(np.int32)

    save_int32_hex(os.path.join(output_dir, 'matmul_a.hex'), A)
    save_int32_hex(os.path.join(output_dir, 'matmul_b.hex'), B)
    save_int32_hex(os.path.join(output_dir, 'matmul_c_expected.hex'), C)

    # Verify
    A2 = load_int32_hex(os.path.join(output_dir, 'matmul_a.hex'))
    B2 = load_int32_hex(os.path.join(output_dir, 'matmul_b.hex'))
    C2 = load_int32_hex(os.path.join(output_dir, 'matmul_c_expected.hex'))

    A2_mat = A2.reshape(dim, dim)
    B2_mat = B2.reshape(dim, dim)
    C_recomputed = (A2_mat.astype(np.int64) @ B2_mat.astype(np.int64)).astype(np.int32)

    ok = (np.array_equal(A, A2) and np.array_equal(B, B2) and
          np.array_equal(C, C2) and np.array_equal(C_recomputed.flatten(), C2))
    print(f"  Verification: {'PASS' if ok else 'FAIL'}")
    return ok


# ============================================================================
# Kernel 3: Element-wise ReLU  Y[i] = max(0, X[i])
# ============================================================================

def generate_relu(output_dir: str, rng: np.random.RandomState,
                  length: int = 16) -> bool:
    """
    Generate test data for a GPU element-wise ReLU kernel.

    Args:
        output_dir: Directory to write .hex files.
        rng:        NumPy random state.
        length:     Vector length (default 16).

    Returns:
        True if verification passes.
    """
    print(f"\n[relu] Generating ReLU test data, length={length}")

    # Mix of positive and negative values
    X = rng.randint(-10000, 10001, size=length).astype(np.int32)
    Y = np.maximum(X, 0).astype(np.int32)

    save_int32_hex(os.path.join(output_dir, 'relu_x.hex'), X)
    save_int32_hex(os.path.join(output_dir, 'relu_y_expected.hex'), Y)

    # Verify
    X2 = load_int32_hex(os.path.join(output_dir, 'relu_x.hex'))
    Y2 = load_int32_hex(os.path.join(output_dir, 'relu_y_expected.hex'))

    Y_recomputed = np.maximum(X2, 0).astype(np.int32)

    ok = (np.array_equal(X, X2) and np.array_equal(Y, Y2) and
          np.array_equal(Y_recomputed, Y2))
    print(f"  Verification: {'PASS' if ok else 'FAIL'}")
    return ok


# ============================================================================
# Main
# ============================================================================

def main():
    parser = argparse.ArgumentParser(
        description="Generate GPU kernel test data (.hex files)."
    )
    parser.add_argument(
        '--output-dir', type=str, default=None,
        help='Directory for output .hex files. '
             'Default: hardware/sim/reference_data/'
    )
    parser.add_argument(
        '--seed', type=int, default=42,
        help='Random seed for reproducibility (default: 42).'
    )
    args = parser.parse_args()

    # Resolve output directory
    if args.output_dir is None:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        project_root = os.path.dirname(script_dir)
        output_dir = os.path.join(project_root, 'hardware', 'sim', 'reference_data')
    else:
        output_dir = args.output_dir

    os.makedirs(output_dir, exist_ok=True)

    print("=" * 60)
    print("Omni-RISC APU — GPU Kernel Test Data Generator")
    print(f"Output directory: {output_dir}")
    print(f"Random seed:      {args.seed}")
    print("=" * 60)

    rng = np.random.RandomState(args.seed)
    all_ok = True

    # Generate test data for each kernel
    all_ok &= generate_vector_add(output_dir, rng, length=16)
    all_ok &= generate_matmul(output_dir, rng, dim=4)
    all_ok &= generate_relu(output_dir, rng, length=16)

    # Summary
    print("\n" + "=" * 60)
    if all_ok:
        print("All GPU kernel test data generated and verified successfully.")
    else:
        print("ERROR: Some verifications failed!")
        sys.exit(1)
    print("=" * 60)


if __name__ == '__main__':
    main()
