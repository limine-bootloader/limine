TARGET = bios

ifeq ($(TARGET), bios)
	TOOLCHAIN=i386-elf
else ifeq ($(TARGET), uefi)
	TOOLCHAIN=x86_64-elf
else
	$(error Invalid target)
endif

CC = $(TOOLCHAIN)-gcc
LD = $(TOOLCHAIN)-ld
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump
READELF = $(TOOLCHAIN)-readelf

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
	-fno-omit-frame-pointer \
	-fno-lto \
	-Wno-address-of-packed-member \
	-masm=intel \
	-mgeneral-regs-only \
	-MMD \
	-DBUILD_ID=$(BUILD_ID) \
	-DLIMINE_VERSION='"$(LIMINE_VERSION)"' \
	-DCOM_OUTPUT=$(COM_OUTPUT) \
	-DE9_OUTPUT=$(E9_OUTPUT) \
	-D$(TARGET)=1 \
	-I. \
	-I..

ifeq ($(TARGET), bios)
	INTERNAL_CFLAGS += \
		-fno-pic
endif

ifeq ($(TARGET), uefi)
	INTERNAL_CFLAGS += \
		-I../gnu-efi/inc \
		-I../gnu-efi/inc/x86_64 \
		-fpic \
		-mno-red-zone
endif

LDFLAGS = -O3 -g

INTERNAL_LDFLAGS := \
	-fno-lto \
	-nostdlib \
	-z max-page-size=0x1000

ifeq ($(TARGET), bios)
	INTERNAL_LDFLAGS += \
		-static \
		-fno-pie \
		-lgcc \
		-static-libgcc
endif

.PHONY: all clean

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ifeq ($(TARGET), bios)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
endif
OBJ := $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o)
HEADER_DEPS := $(C_FILES:.c=.d)

ifeq ($(TARGET), bios)
all: limine_dbg.elf limine.sys stage2.bin stage2.bin.gz
else ifeq ($(TARGET), uefi)
all: BOOTX64.EFI
endif

BOOTX64.EFI: limine_efi.elf
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 limine_efi.elf $@

stage2.bin.gz: stage2.bin
	gzip -n -9 < stage2.bin > stage2.bin.gz

stage2.bin: limine.sys
	dd if=limine.sys bs=$$(( 0x$$($(READELF) -S limine.elf | grep .stage3.text | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of=$@

limine.map.o: limine_nomap.elf
	./gensyms.sh $(OBJDUMP) limine_nomap.elf limine

limine.sys: limine.elf
	$(OBJCOPY) -O binary $< $@

limine_nomap.elf: $(OBJ) font.o sys/smp_trampoline.o
	$(CC) $(OBJ) font.o sys/smp_trampoline.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap.ld -o $@
	$(CC) $(OBJ) font.o sys/smp_trampoline.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Wl,--gc-sections -Tlinker_stage2only.ld -o limine_stage2only.elf || \
		( echo "This error means that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )

limine_dbg.elf: $(OBJ) font.o sys/smp_trampoline.o
	$(CC) $(OBJ) font.o sys/smp_trampoline.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_dbg.ld -o $@

sys/smp_trampoline.o: sys/smp_trampoline.bin
	$(OBJCOPY) -B i8086 -I binary -O default sys/smp_trampoline.bin $@

sys/smp_trampoline.bin: sys/smp_trampoline.real
	nasm $< -f bin -o $@

font.o: font.bin
	$(OBJCOPY) -B i8086 -I binary -O default font.bin $@

limine.elf: $(OBJ) font.o sys/smp_trampoline.o limine.map.o
	$(CC) $(OBJ) font.o sys/smp_trampoline.o limine.map.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o $@

limine_efi.elf: $(OBJ) font.o sys/smp_trampoline.o
	$(LD) -shared -Bsymbolic \
		-T../gnu-efi/gnuefi/elf_x86_64_efi.lds \
		../gnu-efi/x86_64/gnuefi/crt0-efi-x86_64.o \
		../gnu-efi/x86_64/gnuefi/libgnuefi.a \
		../gnu-efi/x86_64/lib/x86_64/efi_stub.o \
		$(OBJ) font.o sys/smp_trampoline.o -o $@

-include $(HEADER_DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

ifeq ($(TARGET), bios)
%.s2.o: %.s2.c
	$(CC) $(S2CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

%.o: %.asm
	nasm $< -F dwarf -g -f elf32 -o $@

clean:
	rm -f limine.elf limine_efi.elf limine_dbg.elf limine_nomap.elf limine_stage2only.elf font.o limine.map.o limine.sys stage2.bin stage2.bin.gz BOOTX64.EFI sys/smp_trampoline.bin sys/smp_trampoline.o $(OBJ) $(HEADER_DEPS)
