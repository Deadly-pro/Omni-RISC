# Omni-RISC APU — Top-Level Makefile
# Usage:
#   make sim TB=cpu/tb_alu        — Run a single testbench
#   make compliance               — Run RISC-V compliance tests
#   make firmware                 — Cross-compile all firmware
#   make synth                    — Run Vivado synthesis
#   make clean                    — Remove build artifacts

.PHONY: sim compliance firmware synth clean help

SHELL := /bin/bash
SIM_DIR := hardware/sim
FW_DIR  := firmware
VIVADO_DIR := hardware/vivado
# Vivado install path — override with `make VIVADO=/path/to/vivado`
VIVADO  ?= /tools/2026.1/Vivado/bin/vivado

help:
	@echo "Omni-RISC APU Build System"
	@echo ""
	@echo "  make sim TB=cpu/tb_alu    Run a Verilator testbench"
	@echo "  make compliance           Run RISC-V compliance tests"
	@echo "  make firmware             Cross-compile firmware"
	@echo "  make synth                Run Vivado synthesis (VIVADO=$(VIVADO))"
	@echo "  make clean                Remove build artifacts"

sim:
ifndef TB
	@echo "Error: specify testbench with TB=<path>, e.g., make sim TB=cpu/tb_alu"
	@exit 1
endif
	@cd $(SIM_DIR) && bash run_sim.sh $(TB)

compliance:
	@cd tests && bash run_compliance.sh

firmware:
	@$(MAKE) -C $(FW_DIR)

synth:
	$(MAKE) -C $(FW_DIR) APP=benchmark_gpu
	python3 scripts/gpu_asm.py firmware/gpu_kernels/gpu_demo.S -o firmware/gpu_kernels/gpu_demo.hex
	@cd $(VIVADO_DIR) && $(VIVADO) -mode batch -source synth.tcl

clean:
	rm -rf obj_dir/ *.vcd *.fst
	rm -rf $(SIM_DIR)/obj_dir/
	rm -rf $(VIVADO_DIR)/project_* $(VIVADO_DIR)/vivado_*.log $(VIVADO_DIR)/synth.runs $(VIVADO_DIR)/synth.gen $(VIVADO_DIR)/.Xil $(VIVADO_DIR)/reports
	$(MAKE) -C $(FW_DIR) clean 2>/dev/null || true
