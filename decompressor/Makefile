CC = i386-elf-gcc
LD = i386-elf-gcc
OBJCOPY = i386-elf-objcopy
BUILDDIR = .

CFLAGS = -flto -Os -pipe -Wall -Wextra -Werror

INTERNAL_CFLAGS = \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-pic \
	-fomit-frame-pointer \
	-Wno-address-of-packed-member \
	-masm=intel \
	-mgeneral-regs-only \
	-MMD \
	-I.

LDFLAGS = -flto -Os

INTERNAL_LDFLAGS = \
	-lgcc \
	-static-libgcc \
	-nostdlib \
	-no-pie \
	-z max-page-size=0x1000 \
	-static \
	-Tlinker.ld

.PHONY: all clean

C_FILES := $(shell find -L ./ -type f -name '*.c' | sort)
ASM_FILES := $(shell find -L ./ -type f -name '*.asm' | sort)
OBJ := $(addprefix $(BUILDDIR)/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
HEADER_DEPS := $(addprefix $(BUILDDIR)/, $(C_FILES:.c=.d))

all: | builddir $(BUILDDIR)/decompressor.bin

builddir:
	for i in $(OBJ); do mkdir -p `dirname $$i`; done

$(BUILDDIR)/decompressor.bin: $(OBJ)
	$(LD) $(OBJ) $(LDFLAGS) $(INTERNAL_LDFLAGS) -o $(BUILDDIR)/decompressor.elf
	$(OBJCOPY) -O binary $(BUILDDIR)/decompressor.elf $@

-include $(HEADER_DEPS)

$(addprefix $(BUILDDIR)/, %.o): %.c
	$(CC) $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

$(addprefix $(BUILDDIR)/, %.o): %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -rf $(BUILDDIR)
