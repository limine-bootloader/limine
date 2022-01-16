TARGET ?=
BUILDDIR ?=
USING_CLANG ?= 0

override SRCDIR := $(shell pwd)

override SPACE := $(subst ,, )

MKESCAPE = $(subst $(SPACE),\ ,$(1))
SHESCAPE = $(subst ','\'',$(1))
OBJESCAPE = $(subst .a ,.a' ',$(subst .o ,.o' ',$(call SHESCAPE,$(1))))

ifeq ($(call MKESCAPE,$(BUILDDIR)), )
	$(error BUILDDIR not specified)
endif

ifeq ($(TARGET), bios)
override OBJCOPY_ARCH := elf32-i386
else ifeq ($(TARGET), uefi64)
override OBJCOPY_ARCH := elf64-x86-64
else ifeq ($(TARGET), uefi32)
override OBJCOPY_ARCH := elf32-i386
else
$(error Invalid target)
endif

ifeq ($(USING_CLANG), 1)
ifeq ($(TARGET), bios)
override LIMINE_CC += --target=i686-elf
endif
ifeq ($(TARGET), uefi64)
override LIMINE_CC += --target=x86_64-elf
endif
ifeq ($(TARGET), uefi32)
override LIMINE_CC += --target=i686-elf
endif
endif

COM_OUTPUT = false
E9_OUTPUT = false

override S2CFLAGS := $(LIMINE_CFLAGS) -Os

override INTERNAL_CFLAGS := \
	-g \
	$(WERROR) \
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
	-DLIMINE_VERSION='"$(LIMINE_VERSION)"' \
	-DLIMINE_COPYRIGHT='"$(LIMINE_COPYRIGHT)"' \
	-DCOM_OUTPUT=$(COM_OUTPUT) \
	-DE9_OUTPUT=$(E9_OUTPUT) \
	-I. \
	-I../stivale \
	-I'$(call SHESCAPE,$(BUILDDIR))/tinf'

ifeq ($(TARGET), bios)
override INTERNAL_CFLAGS += \
		-Dbios=1 \
		-Duefi=0 \
		-m32 \
		-march=i686 \
		-mtune=generic \
		-mabi=sysv \
		-fno-pic \
		-fno-pie
endif

ifeq ($(TARGET), uefi64)
override INTERNAL_CFLAGS32 := \
		$(INTERNAL_CFLAGS) \
		-Dbios=0 \
		-Duefi=1 \
		-m32 \
		-march=i686 \
		-mtune=generic \
		-mabi=sysv \
		-DGNU_EFI_USE_MS_ABI \
		-fpie

override INTERNAL_CFLAGS += \
		-Dbios=0 \
		-Duefi=1 \
		-m64 \
		-march=x86-64 \
		-mtune=generic \
		-mabi=sysv \
		-DGNU_EFI_USE_MS_ABI \
		-I'$(call SHESCAPE,$(BUILDDIR))/reduced-gnu-efi/inc' \
		-I'$(call SHESCAPE,$(BUILDDIR))/reduced-gnu-efi/inc/x86_64' \
		-fpie \
		-mno-red-zone
endif

ifeq ($(TARGET), uefi32)
override INTERNAL_CFLAGS += \
		-Dbios=0 \
		-Duefi=1 \
		-m32 \
		-march=i686 \
		-mtune=generic \
		-mabi=sysv \
		-DGNU_EFI_USE_MS_ABI \
		-I'$(call SHESCAPE,$(BUILDDIR))/reduced-gnu-efi/inc' \
		-I'$(call SHESCAPE,$(BUILDDIR))/reduced-gnu-efi/inc/ia32' \
		-fpie
endif

override INTERNAL_LDFLAGS := \
	-nostdlib \
	-z max-page-size=0x1000

ifeq ($(TARGET), bios)
override INTERNAL_LDFLAGS += \
		-melf_i386 \
		-static \
		--build-id=sha1
endif

ifeq ($(TARGET), uefi64)
override INTERNAL_LDFLAGS += \
		-melf_x86_64 \
		-static \
		-pie \
		--no-dynamic-linker \
		-ztext
endif

ifeq ($(TARGET), uefi32)
override INTERNAL_LDFLAGS += \
		-melf_i386 \
		-static \
		-pie \
		--no-dynamic-linker \
		-ztext
endif

.PHONY: all clean

override C_FILES := $(shell find -L ./ -type f -name '*.c')
ifeq ($(TARGET), bios)
override ASM32_FILES := $(shell find -L ./ -type f -name '*.asm32')
override ASMB_FILES := $(shell find -L ./ -type f -name '*.asmb')

override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM32_FILES:.asm32=.o) $(ASMB_FILES:.asmb=.o) $(C_FILES:.c=.o))
endif
ifeq ($(TARGET), uefi64)
override ASM64_FILES := $(shell find -L ./ -type f -name '*.asm64')
override ASM64U_FILES := $(shell find -L ./ -type f -name '*.asm64u')

override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM64_FILES:.asm64=.o) $(ASM64U_FILES:.asm64u=.o) $(C_FILES:.c=.o))
endif
ifeq ($(TARGET), uefi32)
override ASM32_FILES := $(shell find -L ./ -type f -name '*.asm32')
override ASM32U_FILES := $(shell find -L ./ -type f -name '*.asm32u')

override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM32_FILES:.asm32=.o) $(ASM32U_FILES:.asm32u=.o) $(C_FILES:.c=.o))
endif
override HEADER_DEPS := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.d))

ifeq ($(TARGET), bios)
all: $(call MKESCAPE,$(BUILDDIR))/limine_dbg.elf $(call MKESCAPE,$(BUILDDIR))/limine.sys $(call MKESCAPE,$(BUILDDIR))/stage2.bin.gz
else ifeq ($(TARGET), uefi64)
all: $(call MKESCAPE,$(BUILDDIR))/BOOTX64.EFI
else ifeq ($(TARGET), uefi32)
all: $(call MKESCAPE,$(BUILDDIR))/BOOTIA32.EFI
endif

$(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.bin: sys/smp_trampoline.real
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm $< -f bin -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o: $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.bin
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	cd '$(call SHESCAPE,$(BUILDDIR))/sys' && \
		$(LIMINE_OBJCOPY) -B i8086 -I binary -O $(OBJCOPY_ARCH) smp_trampoline.bin '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/font.o: font.bin
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_OBJCOPY) -B i8086 -I binary -O $(OBJCOPY_ARCH) font.bin '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/tinf-copied: ../tinf/*
	rm -rf '$(call SHESCAPE,$(BUILDDIR))/tinf'
	mkdir -p '$(call SHESCAPE,$(BUILDDIR))'
	cp -r ../tinf '$(call SHESCAPE,$(BUILDDIR))/'
	touch '$(call SHESCAPE,$(BUILDDIR))/tinf-copied'

$(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o: $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$(@:.o=.c))' -o '$(call SHESCAPE,$@)'

ifeq ($(TARGET), bios)

$(call MKESCAPE,$(BUILDDIR))/stage2.bin.gz: $(call MKESCAPE,$(BUILDDIR))/stage2.bin
	gzip -n -9 < '$(call SHESCAPE,$<)' > '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/stage2.bin: $(call MKESCAPE,$(BUILDDIR))/limine.sys
	dd if='$(call SHESCAPE,$<)' bs=$$(( 0x$$("$(LIMINE_READELF)" -S '$(call SHESCAPE,$(BUILDDIR))/limine.elf' | grep .stage3.text | sed 's/^.*] //' | awk '{print $$3}' | sed 's/^0*//') - 0x8000 )) count=1 of='$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/stage2.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_stage2only.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' "$(LIMINE_OBJDUMP)" '$(call SHESCAPE,$<)' stage2 32

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' "$(LIMINE_OBJDUMP)" '$(call SHESCAPE,$<)' full 32

$(call MKESCAPE,$(BUILDDIR))/limine.sys: $(call MKESCAPE,$(BUILDDIR))/limine.elf
	$(LIMINE_OBJCOPY) -O binary '$(call SHESCAPE,$<)' '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine_stage2only.elf: $(OBJ)
	$(LIMINE_LD) '$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_stage2only.ld -o '$(call SHESCAPE,$@)' || \
		( echo "This error may mean that stage 2 was trying to use stage 3 symbols before loading stage 3" && \
		  false )

$(call MKESCAPE,$(BUILDDIR))/limine_nomap.elf: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o $(call MKESCAPE,$(BUILDDIR))/font.o $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o $(call MKESCAPE,$(BUILDDIR))/stage2.map.o
	$(LIMINE_LD) '$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_nomap.ld -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine.elf: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o $(call MKESCAPE,$(BUILDDIR))/font.o $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o $(call MKESCAPE,$(BUILDDIR))/stage2.map.o $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(LIMINE_LD) '$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o '$(call SHESCAPE,$@)'
	$(LIMINE_OBJCOPY) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(LIMINE_OBJCOPY) -B i8086 -I binary -O $(OBJCOPY_ARCH) build-id.s2.bin build-id.s2.o
	$(LIMINE_OBJCOPY) -O binary --only-section=.note.gnu.build-id '$(call SHESCAPE,$@)' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.bin'
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		$(LIMINE_OBJCOPY) -B i8086 -I binary -O $(OBJCOPY_ARCH) build-id.s3.bin build-id.s3.o
	$(LIMINE_LD) '$(call SHESCAPE,$(BUILDDIR))/build-id.s2.o' '$(call SHESCAPE,$(BUILDDIR))/build-id.s3.o' '$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker.ld -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine_dbg.elf: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o $(call MKESCAPE,$(BUILDDIR))/font.o $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o $(call MKESCAPE,$(BUILDDIR))/stage2.map.o $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(LIMINE_LD) '$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -Tlinker_dbg.ld -o '$(call SHESCAPE,$@)'

endif

$(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi: ../reduced-gnu-efi/*
	mkdir -p '$(call SHESCAPE,$(BUILDDIR))/'
	cp -r ../reduced-gnu-efi '$(call SHESCAPE,$(BUILDDIR))/'

ifeq ($(TARGET), uefi64)

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_efi_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' "$(LIMINE_OBJDUMP)" '$(call SHESCAPE,$<)' full 64

$(call MKESCAPE,$(BUILDDIR))/BOOTX64.EFI: $(call MKESCAPE,$(BUILDDIR))/limine_efi.elf
	$(LIMINE_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc -j .sbat --target efi-app-x86_64 '$(call SHESCAPE,$<)' '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/crt0-efi-x86_64.o $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/libgnuefi.a: $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi
	$(MAKE) -C '$(call SHESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi' CC="$(LIMINE_CC) -m64 -march=x86-64 -mtune=generic" AR="$(LIMINE_AR)" ARCH=x86_64

$(call MKESCAPE,$(BUILDDIR))/limine_efi_nomap.elf: $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/crt0-efi-x86_64.o $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o $(call MKESCAPE,$(BUILDDIR))/font.o $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o
	$(LIMINE_LD) \
		-Tlinker_uefi_nomap.ld \
		'$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine_efi.elf: $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/crt0-efi-x86_64.o $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o $(call MKESCAPE,$(BUILDDIR))/font.o $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(LIMINE_LD) \
		-Tlinker_uefi.ld \
		'$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -o '$(call SHESCAPE,$@)'

endif

ifeq ($(TARGET), uefi32)

$(call MKESCAPE,$(BUILDDIR))/full.map.o: $(call MKESCAPE,$(BUILDDIR))/limine_efi_nomap.elf
	cd '$(call SHESCAPE,$(BUILDDIR))' && \
		'$(call SHESCAPE,$(SRCDIR))/gensyms.sh' "$(LIMINE_OBJDUMP)" '$(call SHESCAPE,$<)' full 32

$(call MKESCAPE,$(BUILDDIR))/BOOTIA32.EFI: $(call MKESCAPE,$(BUILDDIR))/limine_efi.elf
	$(LIMINE_OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym -j .rel -j .rela -j .rel.* -j .rela.* -j .reloc -j .sbat --target efi-app-ia32 '$(call SHESCAPE,$<)' '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/crt0-efi-ia32.o $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/libgnuefi.a: $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi
	$(MAKE) -C '$(call SHESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi' CC="$(LIMINE_CC) -m32 -march=i686 -mtune=generic" AR="$(LIMINE_AR)" ARCH=ia32

$(call MKESCAPE,$(BUILDDIR))/limine_efi_nomap.elf: $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/crt0-efi-ia32.o $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o $(call MKESCAPE,$(BUILDDIR))/font.o $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o
	$(LIMINE_LD) \
		-Tlinker_uefi32_nomap.ld \
		'$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/limine_efi.elf: $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/crt0-efi-ia32.o $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi/gnuefi/libgnuefi.a $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o $(call MKESCAPE,$(BUILDDIR))/font.o $(call MKESCAPE,$(BUILDDIR))/sys/smp_trampoline.o $(call MKESCAPE,$(BUILDDIR))/full.map.o
	$(LIMINE_LD) \
		-Tlinker_uefi32.ld \
		'$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -o '$(call SHESCAPE,$@)'

endif

-include $(HEADER_DEPS)

ifeq ($(TARGET), uefi64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/tinf-copied $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET), uefi32)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/tinf-copied $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET), bios)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

-include $(HEADER_DEPS)

ifeq ($(TARGET), bios)
$(call MKESCAPE,$(BUILDDIR))/%.s2.o: %.s2.c
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(S2CFLAGS) $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'
endif

-include $(HEADER_DEPS)

ifeq ($(TARGET), uefi64)
$(call MKESCAPE,$(BUILDDIR))/%.32.o: %.32.c $(call MKESCAPE,$(BUILDDIR))/reduced-gnu-efi
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) $(INTERNAL_CFLAGS32) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@).32'
	$(LIMINE_OBJCOPY) -I elf32-i386 -O elf64-x86-64 '$(call SHESCAPE,$@).32' '$(call SHESCAPE,$@)'
	rm '$(call SHESCAPE,$@).32'
endif

ifeq ($(TARGET), bios)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm32
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -F dwarf -g -Werror -f elf32 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asmb
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -F dwarf -g -Werror -f elf32 -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET), uefi64)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm64
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -F dwarf -g -Werror -f elf64 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm64u
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -F dwarf -g -Werror -f elf64 -o '$(call SHESCAPE,$@)'
endif

ifeq ($(TARGET), uefi32)
$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm32
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -F dwarf -g -Werror -f elf32 -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm32u
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -F dwarf -g -Werror -f elf32 -o '$(call SHESCAPE,$@)'
endif

clean:
	rm -rf '$(call SHESCAPE,$(BUILDDIR))'
