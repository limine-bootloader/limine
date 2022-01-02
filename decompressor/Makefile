BUILDDIR ?=
USING_CLANG ?= 0

override SPACE := $(subst ,, )

MKESCAPE = $(subst $(SPACE),\ ,$(1))
SHESCAPE = $(subst ','\'',$(1))
OBJESCAPE = $(subst .a ,.a' ',$(subst .o ,.o' ',$(call SHESCAPE,$(1))))

ifeq ($(call MKESCAPE,$(BUILDDIR)), )
	$(error BUILDDIR not specified)
endif

TOOLCHAIN ?= limine

TOOLCHAIN_CC ?= $(TOOLCHAIN)-gcc
TOOLCHAIN_LD ?= $(TOOLCHAIN)-ld
TOOLCHAIN_OBJCOPY ?= $(TOOLCHAIN)-objcopy

ifeq ($(shell command -v $(TOOLCHAIN_CC) ; ), )
override TOOLCHAIN_CC := cc
endif
ifeq ($(shell command -v $(TOOLCHAIN_LD) ; ), )
override TOOLCHAIN_LD := ld
endif
ifeq ($(shell command -v $(TOOLCHAIN_OBJCOPY) ; ), )
override TOOLCHAIN_OBJCOPY := objcopy
endif

ifeq ($(USING_CLANG), 1)
override TOOLCHAIN_CC += --target=i686-elf
endif

WERROR ?= -Werror
CFLAGS ?= -Os -pipe -Wall -Wextra $(WERROR)

override INTERNAL_CFLAGS := \
	-m32 \
	-march=i686 \
	-mtune=generic \
	-mabi=sysv \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fno-pie \
	-fomit-frame-pointer \
	-Wno-address-of-packed-member \
	-mno-80387 \
	-mno-mmx \
	-mno-3dnow \
	-mno-sse \
	-mno-sse2 \
	-MMD \
	-I. \
	-I'$(call SHESCAPE,$(BUILDDIR))/tinf'

LDFLAGS ?=

override INTERNAL_LDFLAGS := \
	-melf_i386 \
	-nostdlib \
	-z max-page-size=0x1000 \
	-static \
	-Tlinker.ld

override C_FILES := $(shell find -L ./ -type f -name '*.c')
override ASM_FILES := $(shell find -L ./ -type f -name '*.asm')
override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
override HEADER_DEPS := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.d))

.PHONY: all
all: $(call MKESCAPE,$(BUILDDIR))/decompressor.bin

$(call MKESCAPE,$(BUILDDIR))/decompressor.bin: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o
	$(TOOLCHAIN_LD) '$(call OBJESCAPE,$^)' $(LDFLAGS) $(INTERNAL_LDFLAGS) -o '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf'
	$(TOOLCHAIN_OBJCOPY) -O binary '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf' '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/tinf-copied: ../tinf/*
	rm -rf '$(call SHESCAPE,$(BUILDDIR))/tinf'
	mkdir -p '$(call SHESCAPE,$(BUILDDIR))'
	cp -r ../tinf '$(call SHESCAPE,$(BUILDDIR))/'
	touch '$(call SHESCAPE,$(BUILDDIR))/tinf-copied'

$(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o: $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(TOOLCHAIN_CC) $(CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$(@:.o=.c))' -o '$(call SHESCAPE,$@)'

-include $(HEADER_DEPS)

$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(TOOLCHAIN_CC) $(CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -f elf32 -o '$(call SHESCAPE,$@)'

.PHONY: clean
clean:
	rm -rf '$(call SHESCAPE,$(BUILDDIR))'
