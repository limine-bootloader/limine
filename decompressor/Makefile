BUILDDIR =

ifeq ($(BUILDDIR), )
	$(error BUILDDIR not specified)
endif

TOOLCHAIN = x86_64-elf

TOOLCHAIN_CC = $(TOOLCHAIN)-gcc
TOOLCHAIN_LD = $(TOOLCHAIN)-ld
TOOLCHAIN_OBJCOPY = $(TOOLCHAIN)-objcopy

ifeq ($(shell command -v $(TOOLCHAIN_CC) ; ), )
TOOLCHAIN_CC := gcc
endif
ifeq ($(shell command -v $(TOOLCHAIN_LD) ; ), )
TOOLCHAIN_LD := ld
endif
ifeq ($(shell command -v $(TOOLCHAIN_OBJCOPY) ; ), )
TOOLCHAIN_OBJCOPY := objcopy
endif

CFLAGS = -Os -pipe -Wall -Wextra -Werror

INTERNAL_CFLAGS = \
	-m32 \
	-march=i386 \
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
	-I.

LDFLAGS =

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

$(BUILDDIR)/decompressor.bin: $(OBJ)
	$(TOOLCHAIN_LD) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $(BUILDDIR)/decompressor.elf
	$(TOOLCHAIN_OBJCOPY) -O binary $(BUILDDIR)/decompressor.elf $@

-include $(HEADER_DEPS)

$(BUILDDIR)/%.o: %.c
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -rf $(BUILDDIR)
