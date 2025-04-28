# Directories
SRC_DIRS := boot kernel drivers/vbe drivers/io
BUILD_DIR := build
ISO_DIR := iso

# Files
SOURCES := $(foreach dir,$(SRC_DIRS),$(wildcard $(dir)/*.c) $(wildcard $(dir)/*.asm))
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(filter %.c,$(SOURCES))) \
        $(patsubst %.asm,$(BUILD_DIR)/%.o,$(filter %.asm,$(SOURCES)))
ISO := myos.iso
KERNEL := myos.bin

# Tools
CC := i686-elf-gcc
AS := nasm
LD := i686-elf-ld
CFLAGS := -m32 -ffreestanding -O2 -Wall -Wextra -Iinclude
ASFLAGS := -f elf32

LDFLAGS := -T linker.ld --allow-multiple-definition

.PHONY: all clean iso

all: $(ISO)

$(BUILD_DIR)/%.o: %.c
	mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: %.asm
	mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(KERNEL): $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

iso: $(ISO)

$(ISO): $(KERNEL) grub/grub.cfg
	mkdir -p $(ISO_DIR)/boot/grub
	cp $(KERNEL) $(ISO_DIR)/boot/
	cp grub/grub.cfg $(ISO_DIR)/boot/grub/
	grub-mkrescue -o $@ $(ISO_DIR)

clean:
	rm -rf $(BUILD_DIR) $(KERNEL) $(ISO_DIR) $(ISO) 