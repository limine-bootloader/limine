ifneq (,)
This makefile requires GNU Make.
endif

TARGET =
BUILDDIR =

ifeq ($(BUILDDIR), )
	$(error BUILDDIR not specified)
endif

ifeq ($(TARGET), bios)
	OBJCOPY_ARCH := elf32-i386
else ifeq ($(TARGET), uefi)
	OBJCOPY_ARCH := elf64-x86-64
else ifeq ($(TARGET), uefi32)
	OBJCOPY_ARCH := elf32-i386
else
	$(error Invalid target)
endif

TOOLCHAIN ?= limine

TOOLCHAIN_CC ?= $(TOOLCHAIN)-gcc
TOOLCHAIN_LD ?= $(TOOLCHAIN)-ld
TOOLCHAIN_AR ?= $(TOOLCHAIN)-ar
TOOLCHAIN_OBJCOPY ?= $(TOOLCHAIN)-objcopy
TOOLCHAIN_OBJDUMP ?= $(TOOLCHAIN)-objdump
TOOLCHAIN_READELF ?= $(TOOLCHAIN)-readelf

ifeq ($(shell command -v $(TOOLCHAIN_CC) ; ), )
override TOOLCHAIN_CC := cc
endif
ifeq ($(shell command -v $(TOOLCHAIN_LD) ; ), )
override TOOLCHAIN_LD := ld
endif
ifeq ($(shell command -v $(TOOLCHAIN_AR) ; ), )
override TOOLCHAIN_AR := ar
endif
ifeq ($(shell command -v $(TOOLCHAIN_OBJCOPY) ; ), )
override TOOLCHAIN_OBJCOPY := objcopy
endif
ifeq ($(shell command -v $(TOOLCHAIN_OBJDUMP) ; ), )
override TOOLCHAIN_OBJDUMP := objdump
endif
ifeq ($(shell command -v $(TOOLCHAIN_READELF) ; ), )
override TOOLCHAIN_READELF := readelf
endif

COM_OUTPUT = false
E9_OUTPUT = false

BUILD_ID := $(shell dd if=/dev/urandom count=8 bs=1 2>/dev/null | od -An -t x4 | sed 's/^ /0x/g;s/ //g')
LIMINE_VERSION := $(shell cat ../version 2>/dev/null || ( git describe --exact-match --tags `git log -n1 --pretty='%h'` 2>/dev/null || ( git log -n1 --pretty='%h' && echo -n "(`git branch --show-current`)" ) ) )
LIMINE_COPYRIGHT := $(shell grep Copyright LICENSE.md)

WERROR = -Werror
CFLAGS ?= -O3 -g -pipe -Wall -Wextra $(WERROR)

S2CFLAGS := $(CFLAGS) -Os

INTERNAL_CFLAGS := \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-omit-frame-pointer \
	-fno-lto \
	-Wno-address-of-packed-member \
	-Wshadow \
	-mno-80387 \
	-mno-mmx \
	-mno-3dnow \
	-mno-sse \
	-mno-sse2 \
	-MMD \
	-DBUILD_ID=$(BUILD_ID) \
	-DLIMINE_VERSION='"$(LIMINE_VERSION)"' \
	-DLIMINE_COPYRIGHT='"$(LIMINE_COPYRIGHT)"' \
	-DCOM_OUTPUT=$(COM_OUTPUT) \
	-DE9_OUTPUT=$(E9_OUTPUT) \
	-I. \
	-I..

ifeq ($(TARGET), bios)
	INTERNAL_CFLAGS += \
		-Dbios=1 \
		-Duefi=0 \
		-m32 \
		-march=i386 \
		-mtune=generic \
		-mabi=sysv \
		-fno-pic \
		-fno-pie
endif

ifeq ($(TARGET), uefi)
	INTERNAL_CFLAGS32 := \
		$(INTERNAL_CFLAGS) \
		-Dbios=0 \
		-Duefi=1 \
		-m32 \
		-march=i386 \
		-mtune=generic \
		-mabi=sysv \
		-DGNU_EFI_USE_MS_ABI \
		-fpie

	INTERNAL_CFLAGS += \
		-Dbios=0 \
		-Duefi=1 \
		-m64 \
		-march=x86-64 \
		-mtune=generic \
		-mabi=sysv \
		-DGNU_EFI_USE_MS_ABI \
		-I../gnu-efi/inc \
		-I../gnu-efi/inc/x86_64 \
		-fpie \
		-mno-red-zone
endif

ifeq ($(TARGET), uefi32)
	INTERNAL_CFLAGS += \
		-Dbios=0 \
		-Duefi=1 \
		-m32 \
		-march=i386 \
		-mtune=generic \
		-mabi=sysv \
		-DGNU_EFI_USE_MS_ABI \
		-I../gnu-efi/inc \
		-I../gnu-efi/inc/ia32 \
		-fpie
endif

LDFLAGS ?=

INTERNAL_LDFLAGS := \
	-nostdlib \
	-z max-page-size=0x1000

ifeq ($(TARGET), bios)
	INTERNAL_LDFLAGS += \
		-melf_i386 \
		-static
endif

ifeq ($(TARGET), uefi)
	INTERNAL_LDFLAGS += \
		-melf_x86_64 \
		-static \
		-pie \
		--no-dynamic-linker \
		-ztext
endif

ifeq ($(TARGET), uefi32)
	INTERNAL_LDFLAGS += \
		-melf_i386 \
		-static \
		-pie \
		--no-dynamic-linker \
		-ztext
endif

.PHONY: all clean builddir

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ifeq ($(TARGET), bios)
ASM32_FILES := $(shell find -L ./ -type f -name '*.asm32' | sort)
ASMB_FILES := $(shell find -L ./ -type f -name '*.asmb' | sort)

OBJ := $(addprefix $(BUILDDIR)/, $(ASM32_FILES:.asm32=.o) $(ASMB_FILES:.asmb=.o) $(C_FILES:.c=.o))
endif
ifeq ($(TARGET), uefi)
ASM64_FILES := $(shell find -L ./ -type f -name '*.asm64' | sort)
ASM64U_FILES := $(shell find -L ./ -type f -name '*.asm64u' | sort)

OBJ := $(addprefix $(BUILDDIR)/, $(ASM64_FILES:.asm64=.o) $(ASM64U_FILES:.asm64u=.o) $(C_FILES:.c=.o))
endif
ifeq ($(TARGET), uefi32)
ASM32_FILES := $(shell find -L ./ -type f -name '*.asm32' | sort)
ASM32U_FILES := $(shell find -L ./ -type f -name '*.asm32u' | sort)

OBJ := $(addprefix $(BUILDDIR)/, $(ASM32_FILES:.asm32=.o) $(ASM32U_FILES:.asm32u=.o) $(C_FILES:.c=.o))
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
else ifeq ($(TARGET), uefi32)
all:
	$(MAKE) builddir
	$(MAKE) $(BUILDDIR)/BOOTIA32.EFI
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
	$(TOOLCHAIN_LD) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_stage2only.ld -o $@ || \
		( echo "This error may mean that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )

$(BUILDDIR)/limine_nomap.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o
	$(TOOLCHAIN_LD) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap.ld -o $@

$(BUILDDIR)/limine.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o $(BUILDDIR)/full.map.o
	$(TOOLCHAIN_LD) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o $@

$(BUILDDIR)/limine_dbg.elf: $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/stage2.map.o $(BUILDDIR)/full.map.o
	$(TOOLCHAIN_LD) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_dbg.ld -o $@

endif

$(BUILDDIR)/gnu-efi:
	cp -r ../gnu-efi $(BUILDDIR)/
	# gnu-efi's build system is broken and fails to actually detect clang.
	# This is a workaround.
	sed 's/-maccumulate-outgoing-args//g' < "$(BUILDDIR)/gnu-efi/Make.defaults" > sed.tmp
	mv sed.tmp "$(BUILDDIR)/gnu-efi/Make.defaults"

ifeq ($(TARGET), uefi)

$(BUILDDIR)/full.map.o: $(BUILDDIR)/limine_efi_nomap.elf
	GENSYMS="`pwd`/gensyms.sh" && \
	cd "`dirname $<`" && \
	"$$GENSYMS" $(TOOLCHAIN_OBJDUMP) $< full 64

$(BUILDDIR)/BOOTX64.EFI: $(BUILDDIR)/limine_efi.elf
	$(TOOLCHAIN_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc -j .sbat --target efi-app-x86_64 --subsystem=10 $< $@

$(BUILDDIR)/gnu-efi/gnuefi/crt0-efi-x86_64.o $(BUILDDIR)/gnu-efi/gnuefi/libgnuefi.a: $(BUILDDIR)/gnu-efi
	$(MAKE) -C $(BUILDDIR)/gnu-efi/gnuefi CC="$(TOOLCHAIN_CC) -m64 -march=x86-64 -mtune=generic" AR="$(TOOLCHAIN_AR)" ARCH=x86_64

$(BUILDDIR)/limine_efi_nomap.elf: $(BUILDDIR)/gnu-efi/gnuefi/crt0-efi-x86_64.o $(BUILDDIR)/gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o
	$(TOOLCHAIN_LD) \
		-Tlinker_uefi_nomap.ld \
		$^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

$(BUILDDIR)/limine_efi.elf: $(BUILDDIR)/gnu-efi/gnuefi/crt0-efi-x86_64.o $(BUILDDIR)/gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/full.map.o
	$(TOOLCHAIN_LD) \
		-Tlinker_uefi.ld \
		$^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

endif

ifeq ($(TARGET), uefi32)

$(BUILDDIR)/full.map.o: $(BUILDDIR)/limine_efi_nomap.elf
	GENSYMS="`pwd`/gensyms.sh" && \
	cd "`dirname $<`" && \
	"$$GENSYMS" $(TOOLCHAIN_OBJDUMP) $< full 32

$(BUILDDIR)/BOOTIA32.EFI: $(BUILDDIR)/limine_efi.elf
	$(TOOLCHAIN_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc -j .sbat --target efi-app-ia32 --subsystem=10 $< $@

$(BUILDDIR)/gnu-efi/gnuefi/crt0-efi-ia32.o $(BUILDDIR)/gnu-efi/gnuefi/libgnuefi.a: $(BUILDDIR)/gnu-efi
	$(MAKE) -C $(BUILDDIR)/gnu-efi/gnuefi CC="$(TOOLCHAIN_CC) -m32 -march=i386 -mtune=generic" AR="$(TOOLCHAIN_AR)" ARCH=ia32

$(BUILDDIR)/limine_efi_nomap.elf: $(BUILDDIR)/gnu-efi/gnuefi/crt0-efi-ia32.o $(BUILDDIR)/gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o
	$(TOOLCHAIN_LD) \
		-Tlinker_uefi32_nomap.ld \
		$^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

$(BUILDDIR)/limine_efi.elf: $(BUILDDIR)/gnu-efi/gnuefi/crt0-efi-ia32.o $(BUILDDIR)/gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(BUILDDIR)/font.o $(BUILDDIR)/sys/smp_trampoline.o $(BUILDDIR)/full.map.o
	$(TOOLCHAIN_LD) \
		-Tlinker_uefi32.ld \
		$^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $@

endif

-include $(HEADER_DEPS)

ifeq ($(TARGET), uefi)
$(BUILDDIR)/%.o: %.c $(BUILDDIR)/gnu-efi
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

ifeq ($(TARGET), uefi32)
$(BUILDDIR)/%.o: %.c $(BUILDDIR)/gnu-efi
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

ifeq ($(TARGET), bios)
$(BUILDDIR)/%.o: %.c
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

-include $(HEADER_DEPS)

ifeq ($(TARGET), bios)
$(BUILDDIR)/%.s2.o: %.s2.c
	$(TOOLCHAIN_CC) $(S2CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@
endif

-include $(HEADER_DEPS)

ifeq ($(TARGET), uefi)
$(BUILDDIR)/%.32.o: %.32.c $(BUILDDIR)/gnu-efi
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS32) -c $< -o $@.32
	$(TOOLCHAIN_OBJCOPY) -I elf32-i386 -O elf64-x86-64 $@.32 $@
	rm $@.32
endif

ifeq ($(TARGET), bios)
$(BUILDDIR)/%.o: %.asm32
	nasm $< -F dwarf -g -Werror -f elf32 -o $@

$(BUILDDIR)/%.o: %.asmb
	nasm $< -F dwarf -g -Werror -f elf32 -o $@
endif

ifeq ($(TARGET), uefi)
$(BUILDDIR)/%.o: %.asm64
	nasm $< -F dwarf -g -Werror -f elf64 -o $@

$(BUILDDIR)/%.o: %.asm64u
	nasm $< -F dwarf -g -Werror -f elf64 -o $@
endif

ifeq ($(TARGET), uefi32)
$(BUILDDIR)/%.o: %.asm32
	nasm $< -F dwarf -g -Werror -f elf32 -o $@

$(BUILDDIR)/%.o: %.asm32u
	nasm $< -F dwarf -g -Werror -f elf32 -o $@
endif

clean:
	rm -rf $(BUILDDIR)
