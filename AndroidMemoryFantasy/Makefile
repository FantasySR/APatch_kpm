MYKPM_VERSION := 1.0.0

KP_DIR = ../KernelPatch

NDK_BIN = /usr/local/lib/android/sdk/ndk/29.0.14206865/toolchains/llvm/prebuilt/linux-x86_64/bin
CC = $(NDK_BIN)/aarch64-linux-android31-clang

CFLAGS = -Wall -O2 -fno-PIC -fno-asynchronous-unwind-tables -fno-stack-protector -fno-common -DMYKPM_VERSION=\"$(MYKPM_VERSION)\"
INCLUDE_FLAGS = -I$(KP_DIR)/kernel/. \
                -I$(KP_DIR)/kernel/include \
                -I$(KP_DIR)/kernel/patch/include \
                -I$(KP_DIR)/kernel/linux/include \
                -I$(KP_DIR)/kernel/linux/arch/arm64/include \
                -I$(KP_DIR)/kernel/linux/tools/arch/arm64/include

objs = AMF.o
target = AMF_$(MYKPM_VERSION).kpm

all: $(target)

$(target): $(objs)
	$(CC) -r -o $@ $^

%.o: %.c
	$(CC) $(CFLAGS) $(INCLUDE_FLAGS) -c -o $@ $<

clean:
	rm -f *.o *.kpm