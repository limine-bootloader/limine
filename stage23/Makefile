TARGET = bios

ifeq ($(TARGET), bios)
	TOOLCHAIN=i386-elf
else ifeq ($(TARGET), uefi)
	TOOLCHAIN=x86_64-elf
else
	$(error Invalid toolchain)
endif

CC = $(TOOLCHAIN)-gcc
LD = $(TOOLCHAIN)-gcc
OBJCOPY = $(TOOLCHAIN)-objcopy
OBJDUMP = $(TOOLCHAIN)-objdump
READELF = $(TOOLCHAIN)-readelf

COM_OUTPUT = false
E9_OUTPUT = false

BUILD_ID := $(shell dd if=/dev/urandom count=8 bs=1 | od -An -t x8 | sed 's/^ /0x/')
LIMINE_VERSION := $(shell git describe --exact-match --tags `git log -n1 --pretty='%h'` || git log -n1 --pretty='%h')
WERROR = -Werror
CFLAGS = -Os -pipe -Wall -Wextra $(WERROR)

INTERNAL_CFLAGS := \
	-std=gnu11 \
	-fplan9-extensions \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
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

ifeq ($(TARGET), uefi)
	INTERNAL_CFLAGS += \
		-I../gnu-efi/inc \
		-I../gnu-efi/inc/x86_64 \
		-fpic \
		-fshort-wchar \
		-mno-red-zone \
		-mcmodel=small
endif

LDFLAGS = -Os

INTERNAL_LDFLAGS := \
	-fno-lto \
	-nostdlib \
	-z max-page-size=0x1000

ifeq ($(TARGET), bios)
	INTERNAL_LDFLAGS += \
		-static \
		-no-pie \
		-lgcc \
		-static-libgcc
else ifeq ($(TARGET), uefi)
	INTERNAL_LDFLAGS += \
		-shared \
		-z nocombreloc \
		-Wl,-Bsymbolic
endif

.PHONY: all clean

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ifeq ($(TARGET), bios)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
endif
OBJ := $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o)
ifeq ($(TARGET), uefi)
OBJ += sys/smp_trampoline.o ../gnu-efi/lib/x86_64/efi_stub.o
endif
HEADER_DEPS := $(C_FILES:.c=.d)

ifeq ($(TARGET), bios)
all: limine.sys stage2.bin stage2.bin.gz
else ifeq ($(TARGET), uefi)
all: BOOTX64.EFI
endif

BOOTX64.EFI: limine.elf
	$(OBJCOPY) -I elf64-x86-64 -O efi-app-x86_64 limine.elf $@

stage2.bin.gz: stage2.bin
	gzip -n -9 < stage2.bin > stage2.bin.gz

stage2.bin: limine.sys
	dd if=limine.sys bs=$$(( 0x$$($(READELF) -S limine.elf | grep .stage3.text | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of=$@

limine.map.o: limine_nomap.elf
ifeq ($(TARGET), bios)
	./gensyms.sh $(OBJDUMP) limine_nomap.elf limine
else ifeq ($(TARGET), uefi)
	./gensyms64.sh $(OBJDUMP) limine_nomap.elf limine
endif

limine.sys: limine.elf
	$(OBJCOPY) -O binary $< $@

limine_nomap.elf: $(OBJ)
	$(LD) $(OBJ) font.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap_$(TARGET).ld -o $@
ifeq ($(TARGET), bios)
	$(LD) $(OBJ) font.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Wl,--gc-sections -Tlinker_stage2only.ld -o limine_stage2only.elf || \
		( echo "This error means that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )
endif

font.o:
	$(OBJCOPY) -B i8086 -I binary -O default font.bin $@

limine.elf: $(OBJ) font.o limine.map.o
	$(LD) $(OBJ) font.o limine.map.o $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_$(TARGET).ld -o $@

-include $(HEADER_DEPS)

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
%.o: %.S
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

ifeq ($(TARGET), bios)
%.o: %.asm
	nasm $< -f elf32 -o $@
else ifeq ($(TARGET), uefi)
%.o: %.asm
	nasm $< -f elf64 -o $@
endif

clean:
	rm -f limine.elf limine_nomap.elf limine_stage2only.elf font.o limine.map.o limine.sys stage2.bin stage2.bin.gz BOOTX64.EFI $(OBJ) $(HEADER_DEPS)
