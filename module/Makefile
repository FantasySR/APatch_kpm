NAME = module
VERSION = 1.0

# 使用 GitHub Actions 环境中 NDK 的完整 clang 路径
CLANG = /usr/local/lib/android/sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android31-clang

KDIR = ../KernelPatch/kernel
CFLAGS = -Wall -O2 -fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector -fno-common
CFLAGS += -DMYKPM_VERSION=\"$(VERSION)\"
CFLAGS += -I$(KDIR)/. -I$(KDIR)/include -I$(KDIR)/patch/include
CFLAGS += -I$(KDIR)/linux/include -I$(KDIR)/linux/arch/arm64/include -I$(KDIR)/linux/tools/arch/arm64/include

all:
	$(CLANG) $(CFLAGS) -c -o $(NAME).o $(NAME).c
	$(CLANG) -r -o $(NAME)_$(VERSION).kpm $(NAME).o

clean:
	rm -f *.o *.kpm