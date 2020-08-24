CC = ../toolchain/bin/i386-elf-gcc

CFLAGS = -Os -pipe -Wall -Wextra

INTERNAL_CFLAGS = \
	-std=gnu99 \
	-ffreestanding \
	-masm=intel \
	-fno-pic \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-fno-stack-protector \
	-I. \
	-Wno-address-of-packed-member

LDFLAGS = -O2

INTERNAL_LDFLAGS = \
	-nostdlib \
	-no-pie \
	-lgcc \
	-static-libgcc \
	-Tlinker.ld

.PHONY: all clean

C_FILES := $(shell find ./ -type f -name '*.c' | sort)
OBJ := $(C_FILES:.c=.o)

all: qloader2.bin

qloader2.bin: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o stage2.bin
	cd bootsect && nasm bootsect.asm -fbin -o ../qloader2.bin

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)
