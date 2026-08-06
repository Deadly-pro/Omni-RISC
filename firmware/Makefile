# Omni-RISC APU — Firmware Makefile
# Cross-compiles bare-metal RISC-V firmware.

# Override with make PREFIX=... if your toolchain uses a different triple
PREFIX  ?= riscv64-linux-gnu-
CC      := $(PREFIX)gcc
AS      := $(PREFIX)as
LD      := $(PREFIX)ld
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump

CFLAGS  := -march=rv32im_zicsr_zifencei -mabi=ilp32 -Os -Wall -ffreestanding -nostdlib -MMD -MP
LDFLAGS := -T boot/linker.ld -nostdlib

# Source files
BOOT_SRCS   := boot/startup.S boot/trap_handler.S boot/boot.c
DRIVER_SRCS := drivers/uart.c drivers/timer.c drivers/gpio.c drivers/gpu.c
APP_SRCS    := apps/hello.c apps/timer.c

# Build targets
BOOT_OBJ    := $(BOOT_SRCS:.c=.o)
BOOT_OBJ    := $(BOOT_OBJ:.S=.o)
DRIVER_OBJ  := $(DRIVER_SRCS:.c=.o)
DEPS        := $(BOOT_OBJ:.o=.d) $(DRIVER_OBJ:.o=.d)

# Include auto-generated header dependencies (MMD/MP above)
-include $(DEPS) apps/$(APP).d

# Default app
APP ?= hello

.PHONY: all clean dump

all: $(APP).hex

# Link
$(APP).elf: $(BOOT_OBJ) $(DRIVER_OBJ) apps/$(APP).o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Generate a $readmemh word hex for the BRAMs (flat image from 0x0)
$(APP).hex: $(APP).elf
	$(OBJCOPY) -O binary $< $<.bin
	python3 elf_to_hex.py $<.bin $@

# Disassembly for debugging
dump: $(APP).elf
	$(OBJDUMP) -d -M no-aliases $< > $(APP).dump

# Compile rules
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.elf *.hex *.bin *.dump
	find . -name '*.o' -delete
