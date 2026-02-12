# ====================================================================
#                      Basic Variable Definitions
# ====================================================================
BUILD_DIR := build
SRC_DIR := src

# Busybox-style applets (kept in sync with src/utils/image.mk)
BUSYBOX_APPLETS := ls cat echo env pwd clear date mkdir rmdir rm mount umount mkfs sh dup


# Kernel entry point address
ENTRYPOINT := 0x10000

# Compilers and linkers
CC := gcc
LD := ld
ASM := nasm

# Get libgcc path dynamically (for 64-bit ops on 32-bit)
LIBGCC := $(shell $(CC) -m32 -print-libgcc-file-name)

# ====================================================================
#                   Compilation and Linker Options
# ====================================================================
# C compiler options
CFLAGS := -m32                     # Compile to 32-bit program
CFLAGS += -fno-builtin             # Do not use GCC built-in functions
CFLAGS += -nostdinc                # Do not include standard headers
CFLAGS += -fno-pic                 # Do not generate position-independent code
CFLAGS += -fno-pie                 # Do not generate position-independent executable
CFLAGS += -nostdlib                # Do not link standard library
CFLAGS += -fno-stack-protector     # Disable stack protection
CFLAGS += -g                       # Add debug info
CFLAGS += -I$(SRC_DIR)/include     # Add include path
CFLAGS += -DXJOS

# Assembler options (for kernel)
ASMFLAGS := -f elf32 -g -F dwarf

# Linker options
LDFLAGS := -m elf_i386 -static -z noexecstack
LIB_LDFLAGS := -m elf_i386 -r -z noexecstack


# ====================================================================
#                        内核目标文件列表 (自动生成)
# ====================================================================

# Auto-discover kernel sources under src/kernel.
KERNEL_C_SRCS := $(shell find $(SRC_DIR)/kernel -type f -name '*.c' | LC_ALL=C sort)
KERNEL_ASM_SRCS := $(shell find $(SRC_DIR)/kernel -type f -name '*.asm' | LC_ALL=C sort)

KERNEL_OBJS_AUTO := \
	$(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(KERNEL_C_SRCS)) \
	$(patsubst $(SRC_DIR)/%.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))

# Kernel also links a private build of libs/common with __KERNEL__ defined.
KERNEL_COMMON_C_SRCS := $(shell find $(SRC_DIR)/libs/common -type f -name '*.c' | LC_ALL=C sort)
KERNEL_COMMON_OBJS := $(patsubst $(SRC_DIR)/libs/common/%.c,$(BUILD_DIR)/kernel/common/%.o,$(KERNEL_COMMON_C_SRCS))

# Ensure start.o remains first in the link list.
KERNEL_START_OBJ := $(BUILD_DIR)/kernel/x86/start.o
KERNEL_OBJS := $(KERNEL_START_OBJ) $(filter-out $(KERNEL_START_OBJ),$(KERNEL_OBJS_AUTO) $(KERNEL_COMMON_OBJS))

# ====================================================================
#                        用户态库文件列表 (手动维护)
# ====================================================================
# 对应 libs/ 目录下的通用库
LIB_OBJS := \
	$(BUILD_DIR)/libs/libc/crt.o \
	$(BUILD_DIR)/libs/libc/crt1.o \
	$(BUILD_DIR)/libs/libc/assert.o \
	$(BUILD_DIR)/libs/libc/time.o \
	$(BUILD_DIR)/libs/libc/printf.o \
	$(BUILD_DIR)/libs/libc/syscall.o \
	$(BUILD_DIR)/libs/common/vsprintf.o \
	$(BUILD_DIR)/libs/common/string.o \
	$(BUILD_DIR)/libs/common/stdlib.o \

# ====================================================================
#                           构建规则
# ====================================================================

.PHONY: all clean image

all: image

# 1. 编译 Bootloader
$(BUILD_DIR)/boot/%.bin: $(SRC_DIR)/boot/%.asm
	@mkdir -p $(dir $@)
	@echo "ASM		$<"
	@$(ASM) -f bin $< -o $@

# 2. 编译汇编代码 (Kernel & Libs)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.asm
	@mkdir -p $(dir $@)
	@echo "ASM		$<"
	@$(ASM) $(ASMFLAGS) $< -o $@

# 3. 编译 C 代码 (Kernel & Libs)
$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	@echo "CC		$<"
	@$(CC) $(CFLAGS) -c $< -o $@

# Target-specific compile flags to enforce kernel/user separation
$(BUILD_DIR)/kernel/%.o: CFLAGS += -D__KERNEL__
$(BUILD_DIR)/user/%.o: CFLAGS += -D__USER__
$(BUILD_DIR)/libs/libc/%.o: CFLAGS += -D__USER__
$(BUILD_DIR)/libs/common/%.o: CFLAGS += -D__USER__

# Kernel uses its own build of libs/common with __KERNEL__ defined.
$(BUILD_DIR)/kernel/common/%.o: CFLAGS += -D__KERNEL__
$(BUILD_DIR)/kernel/common/%.o: $(SRC_DIR)/libs/common/%.c
	@mkdir -p $(dir $@)
	@echo "CC\t\t$<"
	@$(CC) $(CFLAGS) -c $< -o $@

# 4. 链接用户态运行时库 (libc.o)
$(BUILD_DIR)/lib/libc.o: $(LIB_OBJS)
	@mkdir -p $(dir $@)
	@echo "LD		$<"
	@$(LD) $(LIB_LDFLAGS) $^ -o $@

$(BUILD_DIR)/user/%.out: \
	$(BUILD_DIR)/user/%.o \
	$(BUILD_DIR)/lib/libc.o
	@mkdir -p $(dir $@)
	@echo "LD       $<"
	@$(LD) $(LDFLAGS) $^ -o $@ -Ttext 0x1001000

# Busybox links multiple applet objects into one binary
BUSYBOX_APPS := $(addprefix $(BUILD_DIR)/user/busybox/applets/,$(addsuffix .o,$(BUSYBOX_APPLETS)))

$(BUILD_DIR)/user/busybox.out: \
	$(BUILD_DIR)/user/busybox/busybox.o \
	$(BUSYBOX_APPS) \
	$(BUILD_DIR)/lib/libc.o
	@mkdir -p $(dir $@)
	@echo "LD       $<"
	@$(LD) $(LDFLAGS) $^ -o $@ -Ttext 0x1001000

# busybox.c includes user/builtin/applets.h
$(BUILD_DIR)/user/busybox/busybox.o: CFLAGS += -I$(SRC_DIR)/user/builtin

# Busybox applet objects are built from user/builtin/*.c with main() disabled.
$(BUILD_DIR)/user/busybox/applets/%.o: CFLAGS += -DXJOS_BUSYBOX_APPLET
$(BUILD_DIR)/user/busybox/applets/%.o: $(SRC_DIR)/user/builtin/%.c
	@mkdir -p $(dir $@)
	@echo "CC\t\t$<"
	@$(CC) $(CFLAGS) -c $< -o $@

BUILTIN_APPS := \
	$(BUILD_DIR)/user/busybox.out \
	$(BUILD_DIR)/user/init.out \

# 5. 链接内核 (kernel.bin)
$(BUILD_DIR)/kernel.bin: $(KERNEL_OBJS)
	@mkdir -p $(dir $@)
	@echo "LD		$<"
	@$(LD) $(LDFLAGS) -Ttext $(ENTRYPOINT) $^ $(LIBGCC) -o $@

# 6. 生成最终镜像
$(BUILD_DIR)/system.bin: $(BUILD_DIR)/kernel.bin
	@echo "OBJCOPY		$<"
	@objcopy -O binary $< $@

$(BUILD_DIR)/system.map: $(BUILD_DIR)/kernel.bin
	@echo "NM		$<"
	@nm $< | sort > $@


include $(SRC_DIR)/utils/image.mk
include $(SRC_DIR)/utils/cmd.mk

clean:
	@echo "CLEAN		$(BUILD_DIR)"
	@rm -rf $(BUILD_DIR)