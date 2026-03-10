# Omni-RISC Project Makefile

# Toolchain settings
# Check for available toolchains: riscv64-linux-gnu- (Debian/Ubuntu) or riscv64-unknown-linux-gnu- (Buildroot/Generic)
ifeq ($(shell which riscv64-linux-gnu-as >/dev/null 2>&1; echo $$?),0)
    CROSS_COMPILE ?= riscv64-linux-gnu-
else
    CROSS_COMPILE ?= riscv64-unknown-linux-gnu-
endif

AS      = $(CROSS_COMPILE)as
LD      = $(CROSS_COMPILE)ld
OBJCOPY = $(CROSS_COMPILE)objcopy

# Simulation settings
VERILATOR = verilator
VERILATOR_FLAGS = -Wall --trace -cc --exe --build -j --Wno-fatal

# Paths
RTL_DIR = hardware/rtl
SIM_DIR = hardware/sim
INC_DIRS = -I$(RTL_DIR) -I$(RTL_DIR)/core -I$(RTL_DIR)/bus -I$(RTL_DIR)/peripherals -I$(RTL_DIR)/mmu -I$(RTL_DIR)/vector

# --- Software Build ---
program.hex: software/test_uart.s
	@echo "🛠 Building test software..."
	$(AS) -march=rv32i -mabi=ilp32 -o software/test_uart.o software/test_uart.s
	$(LD) -m elf32lriscv -Ttext 0x80000000 -o software/test_uart.elf software/test_uart.o
	$(OBJCOPY) -O verilog --verilog-data-width 4 --change-addresses -0x80000000 software/test_uart.elf program.hex
	@echo "✅ program.hex created."

# --- Hardware Build (Verilator) ---
sim_build: $(RTL_DIR)/soc_top.v $(SIM_DIR)/soc_tb.cpp
	@echo "💎 Running Verilator..."
	$(VERILATOR) $(VERILATOR_FLAGS) $(INC_DIRS) \
		$(RTL_DIR)/soc_top.v \
		$(RTL_DIR)/core/*.v \
		$(RTL_DIR)/bus/*.v \
		$(RTL_DIR)/peripherals/*.v \
		--top-module soc_top \
		$(SIM_DIR)/soc_tb.cpp
	@echo "✅ Simulation binary created."

# --- Execution ---
run: program.hex sim_build
	@echo "🚀 Starting Simulation..."
	./obj_dir/Vsoc_top
	@echo "🏁 Simulation finished."

clean:
	rm -rf obj_dir software/*.o software/*.elf *.hex *.vcd
