ifneq (,)
This makefile requires GNU Make.
endif

BUILDDIR =

ifeq ($(BUILDDIR), )
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
	-I$(BUILDDIR)/tinf

LDFLAGS ?=

INTERNAL_LDFLAGS = \
	-melf_i386 \
	-nostdlib \
	-z max-page-size=0x1000 \
	-static \
	-Tlinker.ld

.PHONY: all clean builddir

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
OBJ := $(addprefix $(BUILDDIR)/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
HEADER_DEPS := $(addprefix $(BUILDDIR)/, $(C_FILES:.c=.d))

all:
	$(MAKE) builddir
	$(MAKE) $(BUILDDIR)/decompressor.bin

builddir:
	for i in $(OBJ); do mkdir -p `dirname $$i`; done

$(BUILDDIR)/decompressor.bin: $(OBJ) $(BUILDDIR)/tinf/tinfgzip.o $(BUILDDIR)/tinf/tinflate.o
	$(TOOLCHAIN_LD) $^ $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $(BUILDDIR)/decompressor.elf
	$(TOOLCHAIN_OBJCOPY) -O binary $(BUILDDIR)/decompressor.elf $@

$(BUILDDIR)/tinf-copied: ../tinf/*
	rm -rf $(BUILDDIR)/tinf
	cp -r ../tinf $(BUILDDIR)/
	touch $(BUILDDIR)/tinf-copied

$(BUILDDIR)/tinf/tinfgzip.o $(BUILDDIR)/tinf/tinflate.o: $(BUILDDIR)/tinf-copied
	$(TOOLCHAIN_CC) $(CFLAGS) -Os $(INTERNAL_CFLAGS) -c $(@:.o=.c) -o $@

-include $(HEADER_DEPS)

$(BUILDDIR)/%.o: %.c $(BUILDDIR)/tinf-copied
	$(TOOLCHAIN_CC) $(CFLAGS) -Os $(INTERNAL_CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -rf $(BUILDDIR)
