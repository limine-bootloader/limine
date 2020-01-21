CC = ../toolchain/bin/i386-elf-gcc

CFLAGS = -O2 -pipe -Wall -Wextra

INTERNAL_CFLAGS = \
	-ffreestanding \
	-masm=intel \
	-fno-pic \
	-mno-sse \
	-mno-sse2 \
	-mno-mmx \
	-mno-80387 \
	-ffreestanding \
	-fno-stack-protector \
	-I.

LDFLAGS = -O2

INTERNAL_LDFLAGS = \
	-nostdlib \
	-no-pie \
	-lgcc \
	-static-libgcc \
	-Tlinker.ld

.PHONY: all clean

C_FILES := $(shell find ./ -type f -name '*.c')
OBJ := $(C_FILES:.c=.o)

all: qloader2.bin

qloader2.bin: bootsect/bootsect.bin $(OBJ)
	$(CC) $(LDFLAGS) $(INTERNAL_LDFLAGS) $(OBJ) -o stage2.bin
	cat bootsect/bootsect.bin stage2.bin > $@

bootsect/bootsect.bin: bootsect/bootsect.asm
	cd bootsect && nasm bootsect.asm -fbin -o bootsect.bin

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) bootsect/bootsect.bin
