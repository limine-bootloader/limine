CC = i386-elf-gcc
LD = i386-elf-gcc
OBJCOPY = i386-elf-objcopy
OBJDUMP = i386-elf-objdump
READELF = i386-elf-readelf

BUILD_ID := $(shell dd if=/dev/urandom count=8 bs=1 | od -An -t x8 | sed 's/^ /0x/')
LIMINE_VERSION := $(shell git describe --exact-match --tags `git log -n1 --pretty='%h'` || git log -n1 --pretty='%h')
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
	-DBUILD_ID=$(BUILD_ID) \
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
	-static

.PHONY: all clean

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
OBJ := $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o)
HEADER_DEPS := $(C_FILES:.c=.d)

all: limine.sys stage2.bin stage2.bin.gz

stage2.bin.gz: stage2.bin
	gzip -n -9 < stage2.bin > stage2.bin.gz

stage2.bin: limine.sys
	dd if=limine.sys bs=$$(( 0x$$($(READELF) -S limine.elf | grep .stage3.text | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of=$@

limine.map.o: limine_nomap.elf
	./gensyms.sh $(OBJDUMP) limine_nomap.elf limine

limine.sys: limine.elf
	$(OBJCOPY) -O binary $< $@

limine_nomap.elf: $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -Wl,--gc-sections -Tlinker_stage2only.ld -o $@ || \
		( echo "This error means that stage2 was trying to use stage3 symbols before loading stage 3" && \
		  false )
	$(LD) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap.ld -o $@

limine.elf: $(OBJ) limine.map.o
	$(LD) $(OBJ) limine.map.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o $@

-include $(HEADER_DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -f limine.elf limine_nomap.elf limine.map.o limine.sys stage2.bin stage2.bin.gz $(OBJ) $(HEADER_DEPS)
