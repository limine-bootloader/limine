CC = i386-elf-gcc
LD = i386-elf-gcc
OBJCOPY = i386-elf-objcopy
OBJDUMP = i386-elf-objdump
READELF = i386-elf-readelf

LIMINE_VERSION := $(shell git branch --show-current | sed 's/-branch//')
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
	-DLIMINE_VERSION='"$(LIMINE_VERSION)"' \
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

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
OBJ := $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o)
HEADER_DEPS := $(C_FILES:.c=.d)

all: stages.map stages.bin stage2.bin stage2.bin.gz stage3.bin

stage2.bin.gz: stage2.bin
	gzip -n -9 < stage2.bin > stage2.bin.gz

stage2.bin: stages.bin
	dd if=stages.bin bs=$$(( 0x$$($(READELF) -S stages.elf | grep .stage3 | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of=$@

stage3.bin: stages.bin
	dd if=stages.bin bs=$$(( 0x$$($(READELF) -S stages.elf | grep .stage3 | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) skip=1 of=$@

stages.map: stages.elf
	./gensyms.sh $(OBJDUMP)
	nasm symlist.gen -f bin -o $@

stages.bin: stages.elf
	$(OBJCOPY) -O binary $< $@

stages.elf: $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

-include $(HEADER_DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -f symlist.gen stages.elf stages.map stages.bin stage2.bin stage2.bin.gz stage3.bin $(OBJ) $(HEADER_DEPS)
