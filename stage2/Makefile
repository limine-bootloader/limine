CC = i386-elf-gcc
LD = i386-elf-gcc
OBJCOPY = i386-elf-objcopy
OBJDUMP = i386-elf-objdump

WERROR = -Werror
CFLAGS = -Os -pipe -Wall -Wextra $(WERROR)

INTERNAL_CFLAGS = \
	-std=gnu11 \
	-fplan9-extensions \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-omit-frame-pointer \
	-Wno-address-of-packed-member \
	-masm=intel \
	-mno-80387 \
	-mno-mmx \
	-mno-3dnow \
	-mno-sse \
	-mno-sse2 \
	-MMD \
	-I. \
	-I..

LDFLAGS = -Os

INTERNAL_LDFLAGS = \
	-lgcc \
	-static-libgcc \
	-nostdlib \
	-no-pie \
	-z max-page-size=0x1000 \
	-static \
	-Tlinker.ld

.PHONY: all clean

C_FILES := $(shell find ./ -type f -name '*.c' | sort)
ASM_FILES := $(shell find ./ -type f -name '*.asm' | sort)
OBJ := $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o)
HEADER_DEPS := $(C_FILES:.c=.d)

all: stage2.map stage2.bin

stage2.map: stage2.elf
	./gensyms.sh $(OBJDUMP)
	nasm symlist.gen -f bin -o $@

stage2.bin: stage2.elf
	$(OBJCOPY) -O binary $< $@

stage2.elf: $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

-include $(HEADER_DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -f symlist.gen stage2.map stage2.bin stage2.elf $(OBJ) $(HEADER_DEPS)
