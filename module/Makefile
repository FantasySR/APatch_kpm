NAME = mod
VERSION = 1.0

# NDK clang 路径（GitHub Actions 环境固定）
CLANG = /usr/local/lib/android/sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android31-clang

# KernelPatch 头文件路径
KPATH = ../KernelPatch
CFLAGS = -Wall -O2 -fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector -fno-common
CFLAGS += -DMYKPM_VERSION=\"$(VERSION)\"
CFLAGS += -I$(KPATH)/kernel/. -I$(KPATH)/kernel/include -I$(KPATH)/kernel/patch/include
CFLAGS += -I$(KPATH)/kernel/linux/include -I$(KPATH)/kernel/linux/arch/arm64/include -I$(KPATH)/kernel/linux/tools/arch/arm64/include

all:
	$(CLANG) $(CFLAGS) -c -o $(NAME).o $(NAME).c
	$(CLANG) -r -o $(NAME)_$(VERSION).kpm $(NAME).o

clean:
	rm -f *.o *.kpm