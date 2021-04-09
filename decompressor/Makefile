BUILDDIR =

ifeq ($(BUILDDIR), )
	$(error BUILDDIR not specified)
endif

TOOLCHAIN = x86_64-elf

TOOLCHAIN_CC = $(TOOLCHAIN)-gcc
TOOLCHAIN_OBJCOPY = $(TOOLCHAIN)-objcopy

ifeq ($(shell which $(TOOLCHAIN_CC)), )
TOOLCHAIN_CC := gcc
endif
ifeq ($(shell which $(TOOLCHAIN_OBJCOPY)), )
TOOLCHAIN_OBJCOPY := objcopy
endif

CFLAGS = -flto -Os -pipe -Wall -Wextra -Werror

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
	-masm=intel \
	-mgeneral-regs-only \
	-MMD \
	-I.

LDFLAGS = -flto -Os

INTERNAL_LDFLAGS = \
	-m32 \
	-march=i386 \
	-Wl,-melf_i386 \
	-nostdlib \
	-no-pie \
	-fno-pic \
	-fno-pie \
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
	$(TOOLCHAIN_CC) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $(BUILDDIR)/decompressor.elf
	$(TOOLCHAIN_OBJCOPY) -O binary $(BUILDDIR)/decompressor.elf $@

-include $(HEADER_DEPS)

$(BUILDDIR)/%.o: %.c
	$(TOOLCHAIN_CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

$(BUILDDIR)/%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -rf $(BUILDDIR)
