override MAKEFLAGS += -rR

include $(TOOLCHAIN_FILE)

BUILDDIR ?=

override SPACE := $(subst ,, )

MKESCAPE = $(subst $(SPACE),\ ,$(1))
SHESCAPE = $(subst ','\'',$(1))
OBJESCAPE = $(subst .a ,.a' ',$(subst .o ,.o' ',$(call SHESCAPE,$(1))))

ifeq ($(call MKESCAPE,$(BUILDDIR)),)
    $(error BUILDDIR not specified)
endif

override FREESTANDING_CFLAGS += \
    -Os \
    -std=gnu11 \
    -nostdinc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fomit-frame-pointer \
    -fno-strict-aliasing \
    -fno-lto \
    -fno-pie \
    -fno-pic \
    -m32 \
    -march=i686 \
    -mabi=sysv \
    -mno-80387 \
    -Wshadow \
    -Wvla

override FREESTANDING_CPPFLAGS := \
    -I../freestanding-headers \
    -I./tinf \
    -I. \
    $(FREESTANDING_CPPFLAGS) \
    -MMD

override FREESTANDING_LDFLAGS += \
    -m elf_i386 \
    -nostdlib \
    -z max-page-size=0x1000 \
    -static \
    -T linker.ld

ifeq ($(FREESTANDING_LD_HAS_NO_PIE),yes)
    override FREESTANDING_LDFLAGS += -no-pie
endif

override C_FILES := $(shell find . -type f -name '*.c')
override ASM_FILES := $(shell find . -type f -name '*.asm')
override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
override HEADER_DEPS := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.d))

.PHONY: all
all: $(call MKESCAPE,$(BUILDDIR))/decompressor.bin

$(call MKESCAPE,$(BUILDDIR))/decompressor.bin: $(OBJ) ../libgcc-binaries/libgcc-i686.a
	$(FREESTANDING_LD) '$(call OBJESCAPE,$^)' $(FREESTANDING_LDFLAGS) -o '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf'
	$(FREESTANDING_OBJCOPY) -O binary '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf' '$(call SHESCAPE,$@)'

-include $(HEADER_DEPS)

$(call MKESCAPE,$(BUILDDIR))/%.o: %.c
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(FREESTANDING_CC) $(FREESTANDING_CFLAGS) $(FREESTANDING_CPPFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -f elf32 -o '$(call SHESCAPE,$@)'
