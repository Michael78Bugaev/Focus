# Определение переменных
CC = gcc
AS = nasm
LD = ld
CFLAGS= -c -g -D ATA_DEBUG=0 -D FAT_DEBUG=0 -fcommon -Werror -Wimplicit -w -I include/ -ffreestanding -m32 -march=i486 -mtune=i486 -fno-inline-functions -O2 -fno-omit-frame-pointer
CPPFLAGS=-c -g -fcommon -Werror -w -I ./include/ -ffreestanding -m32 -fno-inline-functions -O2 -fno-omit-frame-pointer
ASMFLAGS=-f elf32
LDFLAGS= -T link.ld --allow-multiple-definition -m elf_i386

# Определение директорий
SRC_DIR = .
BUILD_DIR = build

# Определение файлов
C_SOURCES = $(wildcard $(SRC_DIR)/*.c $(SRC_DIR)/*/*.c $(SRC_DIR)/*/*/*.c)
ASM_SOURCES = $(wildcard $(SRC_DIR)/*.asm $(SRC_DIR)/*/*.asm $(SRC_DIR)/*/*/*.asm)
C_OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(C_SOURCES:.c=.o)))
ASM_OBJECTS = $(addprefix $(BUILD_DIR)/, $(notdir $(ASM_SOURCES:.asm=.asmo)))

# Цель по умолчанию
all: $(BUILD_DIR) $(BUILD_DIR)/kernel

# Создание директории build
$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

# Компилирование C-файлов
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo " CC		" $<
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/*/%.c
	@mkdir -p $(dir $@)
	@echo " CC		" $<
	@$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/*/*/%.c
	@mkdir -p $(dir $@)
	@echo " CC		" $<
	@$(CC) $(CFLAGS) -c $< -o $@

# Компилирование ASM-файлов
$(BUILD_DIR)/%.asmo: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo " AS		" $<
	@$(AS) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.asmo: $(SRC_DIR)/*/%.asm
	@mkdir -p $(dir $@)
	@echo " AS		" $<
	@$(AS) $(ASMFLAGS) $< -o $@

$(BUILD_DIR)/%.asmo: $(SRC_DIR)/*/*/%.asm
	@mkdir -p $(dir $@)
	@echo " AS		" $<
	@$(AS) $(ASMFLAGS) $< -o $@

# Линковка объектных файлов
$(BUILD_DIR)/kernel: $(C_OBJECTS) $(ASM_OBJECTS) | $(BUILD_DIR)
	@echo " LD\t\t$@"
	@$(LD) $(LDFLAGS) -o $@ $^
	@cp $@ ./iso/boot/

# Цель для очистки
clean:
	@rm -rf $(BUILD_DIR)
	@rm -rf ./iso/focus/bin/*

debug:
	@qemu-system-i386 -m 1500M -drive file=../hda.img,format=raw,if=ide -cdrom focusos.iso -boot d -serial stdio -s -S

# ------------ User-space (FEX) toolchain -------------------------

# Directory that will contain user programs (.c -> .fex)
APPS_DIR   = apps

# Library pieces
IMPORTS_S  = $(BUILD_DIR)/imports.S
IMPORTS_O  = $(BUILD_DIR)/imports.o
CRT0_O     = $(BUILD_DIR)/crt0.o
LIBFOCUS_A = $(BUILD_DIR)/libfocus.a

# All C sources inside apps/ become .fex binaries
APP_SRCS = $(wildcard $(APPS_DIR)/*.c)
APP_BINS = $(patsubst %.c,%.fex,$(APP_SRCS))

# Generate imports.S with kernel symbol addresses
$(IMPORTS_S): $(BUILD_DIR)/kernel tools/gen_imports.sh | $(BUILD_DIR)
	@echo " GEN		" $@
	@bash tools/gen_imports.sh $< $@

# Assemble imports.o
$(IMPORTS_O): $(IMPORTS_S)
	@echo " AS		" $@
	@nasm -f elf32 $< -o $@

# Simple startup (crt0) – expected to be in apps/crt0.s
$(CRT0_O): $(APPS_DIR)/crt0.s | $(BUILD_DIR)
	@echo " AS		" $@
	@nasm -f elf32 $< -o $@

# Build static library for user programs
$(LIBFOCUS_A): $(IMPORTS_O) $(CRT0_O)
	@echo " AR		" $@
	@ar rcs $@ $^

# Rule: .c -> .fex (ELF) using user.ld and libfocus.a
$(APPS_DIR)/%.fex: $(APPS_DIR)/%.c $(LIBFOCUS_A) user.ld | $(BUILD_DIR)
	@echo " APP		" $@
	@$(CC) -m32 -O3 -ffreestanding -fno-omit-frame-pointer -fno-pic -no-pie -nostdlib \
	   -mpreferred-stack-boundary=2 -mincoming-stack-boundary=2 \
	   -mno-stackrealign -T user.ld -o $@ $< $(LIBFOCUS_A)

.PHONY: apps
apps: $(LIBFOCUS_A) $(APP_BINS)
	@echo "Built user applications: $(notdir $(APP_BINS))"
	@mkdir -p iso/focus/bin
	@cp $(APPS_DIR)/*.fex iso/focus/bin/
	@$(MAKE) iso

.PHONY: iso
iso: $(BUILD_DIR)/kernel
	@grub-mkrescue -o focusos.iso ./iso
	@echo "ISO refreshed"

.PHONY: run
run: iso
	@qemu-system-i386 -m 1500M -drive file=../hda.img,format=raw,if=ide -cdrom focusos.iso -boot d -serial stdio \
	-netdev user,id=n0 -device e1000,netdev=n0,mac=52:54:00:11:22:33