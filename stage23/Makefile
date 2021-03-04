CC = i386-elf-gcc
LD = i386-elf-gcc
OBJCOPY = i386-elf-objcopy
OBJDUMP = i386-elf-objdump
READELF = i386-elf-readelf

COM_OUTPUT = false
E9_OUTPUT = false

BUILD_ID := $(shell dd if=/dev/urandom count=8 bs=1 | od -An -t x8 | sed 's/^ /0x/')
LIMINE_VERSION := $(shell git describe --exact-match --tags `git log -n1 --pretty='%h'` || git log -n1 --pretty='%h')
WERROR = -Werror
CFLAGS = -O3 -g -pipe -Wall -Wextra $(WERROR)
S2CFLAGS = -Os -g -pipe -Wall -Wextra $(WERROR)

INTERNAL_CFLAGS := \
	-std=gnu11 \
	-fplan9-extensions \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-omit-frame-pointer \
	-Wno-address-of-packed-member \
	-masm=intel \
	-mgeneral-regs-only \
	-MMD \
	-DBUILD_ID=$(BUILD_ID) \
	-DLIMINE_VERSION='"$(LIMINE_VERSION)"' \
	-DCOM_OUTPUT=$(COM_OUTPUT) \
	-DE9_OUTPUT=$(E9_OUTPUT) \
	-I. \
	-I..

LDFLAGS = -O3 -g

INTERNAL_LDFLAGS := \
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

all: limine.sys limine_dbg.elf stage2.bin stage2.bin.gz

stage2.bin.gz: stage2.bin
	gzip -n -9 < stage2.bin > stage2.bin.gz

stage2.bin: limine.sys
	dd if=limine.sys bs=$$(( 0x$$($(READELF) -S limine.elf | grep .stage3.text | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of=$@

limine.map.o: limine_nomap.elf
	./gensyms.sh $(OBJDUMP) limine_nomap.elf limine

limine.sys: limine.elf
	$(OBJCOPY) -O binary $< $@

limine_nomap.elf: $(OBJ) font.o
	$(LD) $(OBJ) font.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap.ld -o $@
	$(LD) $(OBJ) font.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Wl,--gc-sections -Tlinker_stage2only.ld -o limine_stage2only.elf || \
		( echo "This error means that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )

limine_dbg.elf: $(OBJ) font.o
	$(LD) $(OBJ) font.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_dbg.ld -o $@

font.o:
	$(OBJCOPY) -B i8086 -I binary -O default font.bin $@

limine.elf: $(OBJ) font.o limine.map.o
	$(LD) $(OBJ) font.o limine.map.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o $@

-include $(HEADER_DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

ifeq ($(TARGET), bios)
%.s2.o: %.s2.c
	$(CC) $(S2CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -f limine.elf limine_nomap.elf limine_stage2only.elf font.o limine.map.o limine.sys stage2.bin stage2.bin.gz $(OBJ) $(HEADER_DEPS)
