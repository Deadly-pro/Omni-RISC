# Omni-RISC APU — Firmware Makefile
# Cross-compiles bare-metal RISC-V firmware

PREFIX  := riscv32-unknown-elf-
CC      := $(PREFIX)gcc
AS      := $(PREFIX)as
LD      := $(PREFIX)ld
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump

CFLAGS  := -march=rv32im -mabi=ilp32 -Os -Wall -ffreestanding -nostdlib
LDFLAGS := -T boot/linker.ld -nostdlib

# Source files
BOOT_SRCS   := boot/startup.S boot/trap_handler.S boot/boot.c
DRIVER_SRCS := drivers/uart.c drivers/timer.c drivers/gpio.c drivers/gpu.c
APP_SRCS    := apps/hello.c

# Build targets
BOOT_OBJ    := $(BOOT_SRCS:.c=.o)
BOOT_OBJ    := $(BOOT_OBJ:.S=.o)
DRIVER_OBJ  := $(DRIVER_SRCS:.c=.o)

# Default app
APP ?= hello

.PHONY: all clean dump

all: $(APP).hex

# Link
$(APP).elf: $(BOOT_OBJ) $(DRIVER_OBJ) apps/$(APP).o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Generate hex for $readmemh
$(APP).hex: $(APP).elf
	$(OBJCOPY) -O verilog $< $@

# Disassembly for debugging
dump: $(APP).elf
	$(OBJDUMP) -d -M no-aliases $< > $(APP).dump

# Compile rules
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.elf *.hex *.dump
	find . -name '*.o' -delete
