# Omni-RISC APU — Firmware Makefile
# Cross-compiles bare-metal RISC-V firmware.
# Apps: make APP=hello|timer|benchmark_cpu|benchmark_gpu|rtos
#   APP=rtos additionally builds the vendored FreeRTOS kernel and links it.

PREFIX  ?= riscv64-linux-gnu-
CC      := $(PREFIX)gcc
AS      := $(PREFIX)as
LD      := $(PREFIX)ld
OBJCOPY := $(PREFIX)objcopy
OBJDUMP := $(PREFIX)objdump

CFLAGS  := -march=rv32im_zicsr_zifencei -mabi=ilp32 -Os -Wall -ffreestanding -nostdlib -MMD -MP
LDFLAGS := -T boot/linker.ld -nostdlib

BOOT_SRCS   := boot/startup.S boot/trap_handler.S boot/boot.c
DRIVER_SRCS := drivers/uart.c drivers/timer.c drivers/gpio.c drivers/gpu.c

BOOT_OBJ    := $(BOOT_SRCS:.c=.o)
BOOT_OBJ    := $(BOOT_OBJ:.S=.o)
DRIVER_OBJ  := $(DRIVER_SRCS:.c=.o)
DEPS        := $(BOOT_OBJ:.o=.d) $(DRIVER_OBJ:.o=.d)

# --- FreeRTOS (APP=rtos / rtos_q) ------------------------------------------
FREERTOS_ROOT  := third_party/FreeRTOS
FREERTOS_PORT  := $(FREERTOS_ROOT)/portable/GCC/RISC-V
FREERTOS_EXT   := $(FREERTOS_PORT)/chip_specific_extensions/RISCV_MTIME_CLINT_no_extensions
FREERTOS_C_SRC := $(FREERTOS_ROOT)/tasks.c \
                  $(FREERTOS_ROOT)/queue.c \
                  $(FREERTOS_ROOT)/list.c \
                  $(FREERTOS_ROOT)/timers.c \
                  $(FREERTOS_ROOT)/event_groups.c \
                  $(FREERTOS_ROOT)/stream_buffer.c \
                  $(FREERTOS_ROOT)/portable/MemMang/heap_4.c \
                  $(FREERTOS_PORT)/port.c \
                  rtos/omni_libc.c
FREERTOS_C_OBJ := $(FREERTOS_C_SRC:.c=.o)
FREERTOS_ASM_OBJ := $(FREERTOS_PORT)/portASM.o
FREERTOS_INC   := -I$(FREERTOS_ROOT)/include -I$(FREERTOS_PORT) -Irtos -Irtos/include

$(FREERTOS_C_OBJ): CFLAGS += $(FREERTOS_INC) -fno-builtin
apps/rtos.o apps/rtos_q.o: CFLAGS += $(FREERTOS_INC)

$(FREERTOS_ASM_OBJ): FREERTOS_ASM_FLAGS := -march=rv32im_zicsr_zifencei -mabi=ilp32 -c -I$(FREERTOS_PORT) -I$(FREERTOS_EXT)

-include $(DEPS) apps/$(APP).d

APP ?= hello

.PHONY: all clean dump

all: $(APP).hex

# Link
ifeq ($(filter rtos rtos_q,$(APP)),$(APP))
$(APP).elf: $(BOOT_OBJ) $(DRIVER_OBJ) apps/$(APP).o $(FREERTOS_C_OBJ) $(FREERTOS_ASM_OBJ)
	$(CC) $(CFLAGS) -T boot/linker_rtos.ld -nostdlib -o $@ $^
else
$(APP).elf: $(BOOT_OBJ) $(DRIVER_OBJ) apps/$(APP).o
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^
endif

$(APP).hex: $(APP).elf
	$(OBJCOPY) -O binary $< $<.bin
	python3 elf_to_hex.py $<.bin $@

dump: $(APP).elf
	$(OBJDUMP) -d -M no-aliases $< > $(APP).dump

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(FREERTOS_ASM_OBJ): $(FREERTOS_PORT)/portASM.S
	$(CC) $(FREERTOS_ASM_FLAGS) $< -o $@

clean:
	rm -f *.elf *.hex *.bin *.dump
	find . \( -name '*.o' -o -name '*.d' \) -delete
