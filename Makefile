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
VIVADO  := hardware/vivado

help:
	@echo "Omni-RISC APU Build System"
	@echo ""
	@echo "  make sim TB=cpu/tb_alu    Run a Verilator testbench"
	@echo "  make compliance           Run RISC-V compliance tests"
	@echo "  make firmware             Cross-compile firmware"
	@echo "  make synth                Run Vivado synthesis"
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
	@cd $(VIVADO) && vivado -mode batch -source synth.tcl

clean:
	rm -rf obj_dir/ *.vcd *.fst
	rm -rf $(SIM_DIR)/obj_dir/
	$(MAKE) -C $(FW_DIR) clean 2>/dev/null || true
