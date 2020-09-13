CC = ../toolchain/bin/i386-elf-gcc
LD = ../toolchain/bin/i386-elf-gcc

CFLAGS = -flto -Os -pipe -Wall -Wextra

INTERNAL_CFLAGS =  \
	-std=gnu99 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-I. \
	-Wno-address-of-packed-member

LDFLAGS = -flto -Os

INTERNAL_LDFLAGS = \
	-lgcc \
	-static-libgcc \
	-nostdlib \
	-no-pie \
	-static \
	-Tlinker.ld

.PHONY: all clean

C_FILES := $(shell find ./ -type f -name '*.c' | grep -v bootsect | grep -v decompressor | sort)
ASM_FILES := $(shell find ./ -type f -name '*.asm' | grep -v bootsect | grep -v decompressor | sort)
OBJ := $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o)

all: limine.bin

limine.bin: $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o stage2.elf
	objcopy -O binary stage2.elf stage2.bin
	gzip -9 < stage2.bin > stage2.bin.gz
	$(MAKE) -C decompressor
	cd bootsect && nasm bootsect.asm -fbin -o ../limine.bin

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	$(MAKE) -C decompressor clean
	rm -f stage2.bin.gz stage2.bin stage2.elf limine.bin $(OBJ)
