CC = ../../toolchain/bin/i386-elf-gcc

CFLAGS = -Os -pipe -Wall -Wextra

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

all: decompressor.bin

decompressor.bin: $(OBJ)
	$(CC) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ)
