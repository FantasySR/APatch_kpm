NAME = module
VERSION = 1.0

KDIR = ../KernelPatch/kernel
CFLAGS = -Wall -O2 -fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector -fno-common
CFLAGS += -DMYKPM_VERSION=\"$(VERSION)\"
CFLAGS += -I$(KDIR)/. -I$(KDIR)/include -I$(KDIR)/patch/include
CFLAGS += -I$(KDIR)/linux/include -I$(KDIR)/linux/arch/arm64/include -I$(KDIR)/linux/tools/arch/arm64/include

# 使用的编译器 (GitHub Actions 环境中 NDK 的 clang)
CC = aarch64-linux-android31-clang

all:
	$(CC) $(CFLAGS) -c -o $(NAME).o $(NAME).c
	$(CC) -r -o $(NAME)_$(VERSION).kpm $(NAME).o

clean:
	rm -f *.o *.kpm