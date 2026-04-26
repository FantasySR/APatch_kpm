# 模块名称和版本
MYKPM_VERSION := 1.0

# KernelPatch 源码路径（相对路径）
ifndef KP_DIR
    KP_DIR = ../KernelPatch
endif

# 自动检测主机标签（用于 NDK 路径）
OS_NAME = $(shell uname | tr A-Z a-z)
MACHINE = $(shell uname -m)
HOST_TAG = $(OS_NAME)-$(MACHINE)
ifeq ($(HOST_TAG), darwin-arm64)
    HOST_TAG = darwin-x86_64
endif

# NDK 路径（从环境变量或默认路径）
NDK_BIN_DIR := toolchains/llvm/prebuilt/$(HOST_TAG)/bin
ifdef ANDROID_NDK_LATEST_HOME
    NDK_PATH ?= $(ANDROID_NDK_LATEST_HOME)/$(NDK_BIN_DIR)
else ifdef ANDROID_NDK
    NDK_PATH ?= $(ANDROID_NDK)/$(NDK_BIN_DIR)
endif

# 交叉编译工具链
ifdef TARGET_COMPILE
    CC := $(TARGET_COMPILE)gcc
    LD := $(TARGET_COMPILE)ld
else ifdef NDK_PATH
    CC := $(NDK_PATH)/aarch64-linux-android31-clang
    LD := $(NDK_PATH)/ld.lld
endif

# 编译选项
CFLAGS = -Wall -O2 -fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector -fno-common -DMYKPM_VERSION=\"$(MYKPM_VERSION)\"

# 头文件路径（包含 KernelPatch 必要的头文件）
INCLUDE_DIRS := . include patch/include linux/include linux/arch/arm64/include linux/tools/arch/arm64/include
INCLUDE_FLAGS := $(foreach dir,$(INCLUDE_DIRS),-I$(KP_DIR)/kernel/$(dir))

# 源文件（修改这里：将 hosts_redirect.c 换成 AndroidMemoryFantasy.c）
objs := AndroidMemoryFantasy.o

# 生成最终 .kpm 文件名
TARGET := AndroidMemoryFantasy_$(MYKPM_VERSION).kpm

# 编译规则：先编译 .c 为 .o，再链接成 .kpm
all: $(TARGET)

$(TARGET): $(objs)
	$(LD) -r -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c -o $@ $<

clean:
	rm -f *.o *.kpm

.PHONY: all clean