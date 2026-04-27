MYKPM_VERSION := 1.0.0

KP_DIR = ../KernelPatch

# 使用 NDK 自带的 clang 模拟 bare-metal 编译器
NDK_BIN = /usr/local/lib/android/sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/linux-x86_64/bin
CC = $(NDK_BIN)/clang --target=aarch64-none-elf -nostdlib -nostdinc
LD = $(NDK_BIN)/ld.lld

# 编译选项
CFLAGS = -Wall -O2 \
         -fno-PIC \
         -ffreestanding \
         -fno-asynchronous-unwind-tables \
         -fno-stack-protector \
         -fno-common \
         -DMYKPM_VERSION=\"$(MYKPM_VERSION)\"

# 头文件路径
INCLUDE_FLAGS = -I$(KP_DIR)/kernel/. \
                -I$(KP_DIR)/kernel/include \
                -I$(KP_DIR)/kernel/patch/include \
                -I$(KP_DIR)/kernel/linux/include \
                -I$(KP_DIR)/kernel/linux/arch/arm64/include \
                -I$(KP_DIR)/kernel/linux/tools/arch/arm64/include

objs = AMF.o
target = AMF_$(MYKPM_VERSION).kpm

all: $(target)

# 链接步骤：用 ld.lld 代替原来的 clang
$(target): $(objs)
	$(LD) -r -o $@ $^

# 编译步骤
%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c -o $@ $<

clean:
	rm -f *.o *.kpm