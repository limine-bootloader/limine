TARGET =
BUILDDIR =

ifeq ($(BUILDDIR), )
	$(error BUILDDIR not specified)
endif

ifeq ($(TARGET), bios)
	TOOLCHAIN=i386-elf
else ifeq ($(TARGET), uefi)
	TOOLCHAIN=x86_64-elf
else
	$(error Invalid target)
endif

CC32 = i386-elf-gcc

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
		-static-libgcc \
		-Wl,--gc-sections
endif

.PHONY: all clean builddir

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ifeq ($(TARGET), bios)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
endif
ifeq ($(TARGET), uefi)
ASM_FILES := lib/do_32.asm
endif
OBJ := $(addprefix $(BUILDDIR)/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
HEADER_DEPS := $(addprefix $(BUILDDIR)/, $(C_FILES:.c=.d))

ifeq ($(TARGET), bios)
all:
	$(MAKE) builddir
	$(MAKE) $(BUILDDIR)/limine_dbg.elf $(BUILDDIR)/limine.sys $(BUILDDIR)/stage2.bin $(BUILDDIR)/stage2.bin.gz
else ifeq ($(TARGET), uefi)
all:
	$(MAKE) builddir
	$(MAKE) $(BUILDDIR)/BOOTX64.EFI
endif

builddir:
	for i in $(OBJ); do mkdir -p `dirname $$i`; done

$(BUILDDIR)/sys/smp_trampoline.bin: sys/smp_trampoline.real
	nasm $< -f bin -o $@

$(BUILDDIR)/sys/smp_trampoline.o: $(BUILDDIR)/sys/smp_trampoline.bin
	cd "`dirname $<`" && \
		$(OBJCOPY) -B i8086 -I binary -O default "`basename $<`" $@

$(BUILDDIR)/font.o: font.bin
	cd "`dirname $<`" && \
		$(OBJCOPY) -B i8086 -I binary -O default "`basename $<`" $@

ifeq ($(TARGET), bios)

$(BUILDDIR)/stage2.bin.gz: $(BUILDDIR)/stage2.bin
	gzip -n -9 < $< > $@

$(BUILDDIR)/stage2.bin: $(BUILDDIR)/limine.sys
	dd if=$< bs=$$(( 0x$$($(READELF) -S $(BUILDDIR)/limine.elf | grep .stage3.text | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of=$@

$(BUILDDIR)/stage2.map.o: $(BUILDDIR)/limine_stage2only.elf
	GENSYMS="`pwd`/gensyms.sh" && \
	cd "`dirname $<`" && \
	"$$GENSYMS" $(OBJDUMP) $< stage2

$(BUILDDIR)/full.map.o: $(BUILDDIR)/limine_nomap.elf
	GENSYMS="`pwd`/gensyms.sh" && \
	cd "`dirname $<`" && \
	"$$GENSYMS" $(OBJDUMP) $< full

$(BUILDDIR)/limine.sys: $(BUILDDIR)/limine.elf
	$(OBJCOPY) -O binary $< $@

$(BUILDDIR)/limine_stage2only.elf: $(OBJ)
	$(CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_stage2only.ld -o $@ || \
		( echo "This error means that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )

$(BUILDDIR)/limine_nomap.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o
	$(CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap.ld -o $@

$(BUILDDIR)/limine.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o $(BUILDDIR)/full.map.o
	$(CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o $@

$(BUILDDIR)/limine_dbg.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o $(BUILDDIR)/full.map.o
	$(CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_dbg.ld -o $@

endif

ifeq ($(TARGET), uefi)

$(BUILDDIR)/BOOTX64.EFI: $(BUILDDIR)/limine_efi.elf
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $< $@

$(BUILDDIR)/limine_efi.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o
	$(LD) -shared -Bsymbolic \
		-T../gnu-efi/gnuefi/elf_x86_64_efi.lds \
		../gnu-efi/gnuefi/crt0-efi-x86_64.o \
		../gnu-efi/gnuefi/libgnuefi.a \
		../gnu-efi/lib/x86_64/efi_stub.o \
		$^ -o $@

endif

-include $(HEADER_DEPS)

$(BUILDDIR)/%.o: %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

ifeq ($(TARGET), bios)
$(BUILDDIR)/%.s2.o: %.s2.c
	$(CC) $(S2CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

ifeq ($(TARGET), uefi)
$(BUILDDIR)/%.32.o: %.32.c
	$(CC32) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@.32
	$(OBJCOPY) -I elf32-i386 -O elf64-x86-64 $@.32 $@
	rm $@.32
endif

ifeq ($(TARGET), bios)
$(BUILDDIR)/%.o: %.asm
	nasm $< -F dwarf -g -f elf32 -o $@
endif
ifeq ($(TARGET), uefi)
$(BUILDDIR)/%.o: %.asm
	nasm $< -F dwarf -g -f elf64 -o $@
endif

clean:
	rm -rf $(BUILDDIR)
