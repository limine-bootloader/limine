TARGET =
BUILDDIR =

ifeq ($(BUILDDIR), )
	$(error BUILDDIR not specified)
endif

ifeq ($(TARGET), bios)
	OBJCOPY_ARCH = elf32-i386
else ifeq ($(TARGET), uefi)
	OBJCOPY_ARCH = elf64-x86-64
else
	$(error Invalid target)
endif

TOOLCHAIN = x86_64-elf

TOOLCHAIN_CC = $(TOOLCHAIN)-gcc
TOOLCHAIN_OBJCOPY = $(TOOLCHAIN)-objcopy
TOOLCHAIN_OBJDUMP = $(TOOLCHAIN)-objdump
TOOLCHAIN_READELF = $(TOOLCHAIN)-readelf

ifeq ($(shell which $(TOOLCHAIN_CC)), )
TOOLCHAIN_CC := gcc
endif
ifeq ($(shell which $(TOOLCHAIN_OBJCOPY)), )
TOOLCHAIN_OBJCOPY := objcopy
endif
ifeq ($(shell which $(TOOLCHAIN_OBJDUMP)), )
TOOLCHAIN_OBJDUMP := objdump
endif
ifeq ($(shell which $(TOOLCHAIN_READELF)), )
TOOLCHAIN_READELF := readelf
endif

COM_OUTPUT = false
E9_OUTPUT = false

BUILD_ID := $(shell dd if=/dev/urandom count=8 bs=1 2>/dev/null | od -An -t x8 | sed 's/^ /0x/')
LIMINE_VERSION := $(shell git describe --exact-match --tags `git log -n1 --pretty='%h'` 2>/dev/null || ( git log -n1 --pretty='%h' && echo -n "(`git branch --show-current`)" ) )

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
	-fno-pic \
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
		-m32 \
		-march=i386 \
		-fno-pie
endif

ifeq ($(TARGET), uefi)
	INTERNAL_CFLAGS += \
		-m64 \
		-march=x86-64 \
		-I../gnu-efi/inc \
		-I../gnu-efi/inc/x86_64 \
		-fpie \
		-mno-red-zone

	INTERNAL_CFLAGS32 := \
		$(INTERNAL_CFLAGS) \
		-m32 \
		-march=i386 \
		-fpie
endif

LDFLAGS = -O3 -g

INTERNAL_LDFLAGS := \
	-fno-pic \
	-fno-lto \
	-nostdlib \
	-z max-page-size=0x1000

ifeq ($(TARGET), bios)
	INTERNAL_LDFLAGS += \
		-m32 \
		-march=i386 \
		-Wl,-melf_i386 \
		-static \
		-no-pie \
		-fno-pie
endif

ifeq ($(TARGET), uefi)
	INTERNAL_LDFLAGS += \
		-m64 \
		-march=x86-64 \
		-Wl,-melf_x86_64 \
		-static-pie \
		-Wl,-Bsymbolic \
		-Wl,--noinhibit-exec \
		-pie \
		-fpie
endif

.PHONY: all clean builddir

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ifeq ($(TARGET), bios)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
OBJ := $(addprefix $(BUILDDIR)/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
endif
ifeq ($(TARGET), uefi)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm64' | sort)
OBJ := $(addprefix $(BUILDDIR)/, $(ASM_FILES:.asm64=.o) $(C_FILES:.c=.o))
endif
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
		$(TOOLCHAIN_OBJCOPY) -B i8086 -I binary -O $(OBJCOPY_ARCH) "`basename $<`" $@

$(BUILDDIR)/font.o: font.bin
	cd "`dirname $<`" && \
		$(TOOLCHAIN_OBJCOPY) -B i8086 -I binary -O $(OBJCOPY_ARCH) "`basename $<`" $@

ifeq ($(TARGET), bios)

$(BUILDDIR)/stage2.bin.gz: $(BUILDDIR)/stage2.bin
	gzip -n -9 < $< > $@

$(BUILDDIR)/stage2.bin: $(BUILDDIR)/limine.sys
	dd if=$< bs=$$(( 0x$$($(TOOLCHAIN_READELF) -S $(BUILDDIR)/limine.elf | grep .stage3.text | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of=$@

$(BUILDDIR)/stage2.map.o: $(BUILDDIR)/limine_stage2only.elf
	GENSYMS="`pwd`/gensyms.sh" && \
	cd "`dirname $<`" && \
	"$$GENSYMS" $(TOOLCHAIN_OBJDUMP) $< stage2 32

$(BUILDDIR)/full.map.o: $(BUILDDIR)/limine_nomap.elf
	GENSYMS="`pwd`/gensyms.sh" && \
	cd "`dirname $<`" && \
	"$$GENSYMS" $(TOOLCHAIN_OBJDUMP) $< full 32

$(BUILDDIR)/limine.sys: $(BUILDDIR)/limine.elf
	$(TOOLCHAIN_OBJCOPY) -O binary $< $@

$(BUILDDIR)/limine_stage2only.elf: $(OBJ)
	$(TOOLCHAIN_CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_stage2only.ld -o $@ || \
		( echo "This error means that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )

$(BUILDDIR)/limine_nomap.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o
	$(TOOLCHAIN_CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap.ld -o $@

$(BUILDDIR)/limine.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o $(BUILDDIR)/full.map.o
	$(TOOLCHAIN_CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o $@

$(BUILDDIR)/limine_dbg.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o $(BUILDDIR)/full.map.o
	$(TOOLCHAIN_CC) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_dbg.ld -o $@

endif

ifeq ($(TARGET), uefi)

$(BUILDDIR)/full.map.o: $(BUILDDIR)/limine_efi_nomap.elf
	GENSYMS="`pwd`/gensyms.sh" && \
	cd "`dirname $<`" && \
	"$$GENSYMS" $(TOOLCHAIN_OBJDUMP) $< full 64

$(BUILDDIR)/BOOTX64.EFI: $(BUILDDIR)/limine_efi.elf
	$(TOOLCHAIN_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc --target efi-app-x86_64 --subsystem=10 $< $@

$(BUILDDIR)/limine_efi_nomap.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o
	$(TOOLCHAIN_CC) \
		-Tlinker_uefi_nomap.ld \
		../gnu-efi/gnuefi/crt0-efi-x86_64.o \
		../gnu-efi/gnuefi/libgnuefi.a \
		../gnu-efi/lib/x86_64/efi_stub.o \
		$^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

$(BUILDDIR)/limine_efi.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/full.map.o
	$(TOOLCHAIN_CC) \
		-Tlinker_uefi.ld \
		../gnu-efi/gnuefi/crt0-efi-x86_64.o \
		../gnu-efi/gnuefi/libgnuefi.a \
		../gnu-efi/lib/x86_64/efi_stub.o \
		$^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

endif

-include $(HEADER_DEPS)

$(BUILDDIR)/%.o: %.c
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

-include $(HEADER_DEPS)

ifeq ($(TARGET), bios)
$(BUILDDIR)/%.s2.o: %.s2.c
	$(TOOLCHAIN_CC) $(S2CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

-include $(HEADER_DEPS)

ifeq ($(TARGET), uefi)
$(BUILDDIR)/%.32.o: %.32.c
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS32) -c $< -o $@.32
	$(TOOLCHAIN_OBJCOPY) -I elf32-i386 -O elf64-x86-64 $@.32 $@
	rm $@.32
endif

$(BUILDDIR)/%.o: %.asm
	nasm $< -F dwarf -g -f elf32 -o $@

$(BUILDDIR)/%.o: %.asm64
	nasm $< -F dwarf -g -f elf64 -o $@

clean:
	rm -rf $(BUILDDIR)
