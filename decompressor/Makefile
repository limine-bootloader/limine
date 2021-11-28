ifneq (,)
This makefile requires GNU Make.
endif

BUILDDIR =

SPACE := $(subst ,, )

MKESCAPE = $(subst #,\#,$(subst $$,$$$$,$(subst $(SPACE),\ ,$(1))))
SHESCAPE = $(subst ','\'',$(1))
OBJESCAPE = $(subst .a ,.a' ',$(subst .o ,.o' ',$(call SHESCAPE,$(1))))

ifeq ($(call MKESCAPE,$(BUILDDIR)), )
	$(error $(call MKESCAPE,$(BUILDDIR)) not specified)
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

WERROR = -Werror
CFLAGS ?= -Os -pipe -Wall -Wextra $(WERROR)

INTERNAL_CFLAGS = \
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

INTERNAL_LDFLAGS = \
	-melf_i386 \
	-nostdlib \
	-z max-page-size=0x1000 \
	-static \
	-Tlinker.ld

.PHONY: all clean

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
HEADER_DEPS := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.d))

all:
	$(MAKE) '$(call SHESCAPE,$(BUILDDIR))/decompressor.bin'

$(call MKESCAPE,$(BUILDDIR))/decompressor.bin: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o
	$(TOOLCHAIN_LD) '$(call OBJESCAPE,$^)' $(LDFLAGS) $(INTERNAL_LDFLAGS) -o '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf'
	$(TOOLCHAIN_OBJCOPY) -O binary '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf' '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/tinf-copied: ../tinf/*
	rm -rf '$(call SHESCAPE,$(BUILDDIR))/tinf'
	mkdir -p '$(call SHESCAPE,$(BUILDDIR))'
	cp -r ../tinf '$(call SHESCAPE,$(BUILDDIR))/'
	touch '$(call SHESCAPE,$(BUILDDIR))/tinf-copied'

$(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o: $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	dirname '$(call SHESCAPE,$@)' | xargs -d '\n' mkdir -p
	$(TOOLCHAIN_CC) $(CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$(@:.o=.c))' -o '$(call SHESCAPE,$@)'

-include $(HEADER_DEPS)

$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	dirname '$(call SHESCAPE,$@)' | xargs -d '\n' mkdir -p
	$(TOOLCHAIN_CC) $(CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm
	dirname '$(call SHESCAPE,$@)' | xargs -d '\n' mkdir -p
	nasm '$(call SHESCAPE,$<)' -f elf32 -o '$(call SHESCAPE,$@)'

clean:
	rm -rf '$(call SHESCAPE,$(BUILDDIR))'
