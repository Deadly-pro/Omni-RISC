#!/bin/bash
set -e

echo "🚀 Starting Omni-RISC Environment Setup..."

# Ensure we are in the project root
if [ ! -f "setup_linux.sh" ]; then
    echo "❌ Error: Please run this script from the project root (where setup_linux.sh is located)."
    exit 1
fi

# 1. Update and install basic dependencies
echo "📦 Installing system dependencies..."
sudo apt-get update
sudo apt-get install -y \
    autoconf automake autotools-dev curl libmpc-dev libmpfr-dev libgmp-dev \
    gawk build-essential bison flex texinfo gperf libtool patchutils bc \
    zlib1g-dev libexpat-dev libncurses-dev device-tree-compiler \
    python3 python3-pip git wget qemu-system-misc \
    ccache libgoogle-perftools-dev numactl perl-doc \
    libfl-dev libfl2 zlib1g zlib1g-dev \
    rsync cpio unzip libssl-dev libelf-dev cmake pkg-config

# 2. Install Verilator (Latest stable)
echo "💎 Installing Verilator..."
if ! command -v verilator &> /dev/null; then
    sudo apt-get install -y verilator
else
    echo "Verilator is already installed ($(verilator --version | head -n 1))."
fi

# 3. Create project directories and clone components
echo "📂 Setting up project structure..."
mkdir -p hardware/{rtl,sim,tb}
mkdir -p software/{kernel,opensbi,uboot}
mkdir -p scripts
mkdir -p docs

if [ ! -d "software/buildroot/.git" ]; then
    echo "📥 Cloning Buildroot (this may take a moment)..."
    if [ -d "software/buildroot" ] && [ "$(ls -A software/buildroot)" ]; then
        echo "⚠️  'software/buildroot' already exists and is not empty. Skipping clone."
    else
        git clone --depth 1 https://github.com/buildroot/buildroot.git software/buildroot
    fi
else
    echo "✅ Buildroot already exists."
fi

# 4. Check for RISC-V Toolchain
echo "🔍 Checking for RISC-V toolchain..."
# Note: For the Top-Down flow (RV32IMV), Buildroot will build its own specific toolchain.
if ! command -v riscv64-unknown-linux-gnu-gcc &> /dev/null; then
    echo "⚠️  RISC-V host toolchain NOT found in PATH."
    echo "💡 Note: Buildroot will generate the necessary toolchain in Phase 1."
    echo "   To install a host toolchain for quick tests: 'sudo apt install gcc-riscv64-unknown-linux-gnu'"
else
    echo "✅ RISC-V toolchain detected: $(riscv64-unknown-linux-gnu-gcc --version | head -n 1)"
fi

echo "✨ Setup complete! You are ready to start the Top-Down flow."
echo "Suggested next step: Configure Buildroot in 'software/buildroot' (e.g., 'make menuconfig')."
