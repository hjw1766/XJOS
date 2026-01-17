# ====================================================================
#                      Basic Variable Definitions
# ====================================================================
BUILD_DIR := build
SRC_DIR := src

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
#                        内核目标文件列表 (手动维护)
# ====================================================================

KERNEL_OBJS := \
	$(BUILD_DIR)/kernel/x86/start.o \
	$(BUILD_DIR)/kernel/init/main.o \
	$(BUILD_DIR)/kernel/init/global.o \
	$(BUILD_DIR)/kernel/init/osh.o \
	$(BUILD_DIR)/kernel/x86/io.o \
	$(BUILD_DIR)/kernel/x86/handler.o \
	$(BUILD_DIR)/kernel/x86/schedule.o \
	$(BUILD_DIR)/kernel/interrupt/interrupt.o \
	$(BUILD_DIR)/kernel/interrupt/clock.o \
	$(BUILD_DIR)/kernel/driver/console.o \
	$(BUILD_DIR)/kernel/driver/device.o \
	$(BUILD_DIR)/kernel/driver/ide.o \
	$(BUILD_DIR)/kernel/driver/keyboard.o \
	$(BUILD_DIR)/kernel/driver/ramdisk.o \
	$(BUILD_DIR)/kernel/driver/rtc.o \
	$(BUILD_DIR)/kernel/driver/serial.o \
	$(BUILD_DIR)/kernel/mm/memory.o \
	$(BUILD_DIR)/kernel/mm/arena.o \
	$(BUILD_DIR)/kernel/time/time.o \
	$(BUILD_DIR)/kernel/task/task.o \
	$(BUILD_DIR)/kernel/task/thread.o \
	$(BUILD_DIR)/kernel/task/sched.o \
	$(BUILD_DIR)/kernel/task/execve.o \
	$(BUILD_DIR)/kernel/syscall/gate.o \
	$(BUILD_DIR)/kernel/sync/mutex.o \
	$(BUILD_DIR)/kernel/sync/semaphore.o \
	$(BUILD_DIR)/kernel/sync/spinlock.o \
	$(BUILD_DIR)/kernel/fs/super.o \
	$(BUILD_DIR)/kernel/fs/bmap.o \
	$(BUILD_DIR)/kernel/fs/inode.o \
	$(BUILD_DIR)/kernel/fs/namei.o \
	$(BUILD_DIR)/kernel/fs/file.o \
	$(BUILD_DIR)/kernel/fs/stat.o \
	$(BUILD_DIR)/kernel/fs/dev.o \
	$(BUILD_DIR)/kernel/fs/buffer.o \
	$(BUILD_DIR)/kernel/fs/system.o \
	$(BUILD_DIR)/libs/common/string.o \
	$(BUILD_DIR)/libs/common/stdlib.o \
	$(BUILD_DIR)/libs/common/printf.o \
	$(BUILD_DIR)/libs/common/vsprintf.o \
	$(BUILD_DIR)/libs/common/syscall.o \
	$(BUILD_DIR)/kernel/lib/bitmap.o \
	$(BUILD_DIR)/kernel/lib/debug.o \
	$(BUILD_DIR)/kernel/lib/printk.o \
	$(BUILD_DIR)/kernel/lib/rbtree.o \
	$(BUILD_DIR)/kernel/lib/assert.o

# ====================================================================
#                        用户态库文件列表 (手动维护)
# ====================================================================
# 对应 libs/ 目录下的通用库
LIB_OBJS := \
	$(BUILD_DIR)/libs/libc/crt.o \
	$(BUILD_DIR)/libs/libc/crt1.o \
	$(BUILD_DIR)/libs/libc/assert.o \
	$(BUILD_DIR)/libs/libc/time.o \
	$(BUILD_DIR)/libs/common/string.o \
	$(BUILD_DIR)/libs/common/stdlib.o \
	$(BUILD_DIR)/libs/common/printf.o \
	$(BUILD_DIR)/libs/common/vsprintf.o \
	$(BUILD_DIR)/libs/common/syscall.o \

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

# 4. 链接用户态运行时库 (libc.o)
$(BUILD_DIR)/lib/libc.o: $(LIB_OBJS)
	@mkdir -p $(dir $@)
	@echo "LD		$<"
	@$(LD) $(LIB_LDFLAGS) $^ -o $@

$(BUILD_DIR)/kernel/builtin/%.out: \
	$(BUILD_DIR)/kernel/builtin/%.o \
	$(BUILD_DIR)/lib/libc.o
	@mkdir -p $(dir $@)
	@echo "LD       $<"
	@$(LD) $(LDFLAGS) $^ -o $@ -Ttext 0x1001000

BUILTIN_APPS := \
	$(BUILD_DIR)/kernel/builtin/env.out \
	$(BUILD_DIR)/kernel/builtin/echo.out \
	$(BUILD_DIR)/kernel/builtin/cat.out \
	$(BUILD_DIR)/kernel/builtin/ls.out \

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