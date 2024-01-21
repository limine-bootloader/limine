override MAKEFLAGS += -rR

include $(TOOLCHAIN_FILE)
export CC_FOR_TARGET
export LD_FOR_TARGET
export OBJDUMP_FOR_TARGET
export OBJCOPY_FOR_TARGET
export READELF_FOR_TARGET

BUILDDIR ?=

override SPACE := $(subst ,, )

MKESCAPE = $(subst $(SPACE),\ ,$(1))
SHESCAPE = $(subst ','\'',$(1))
OBJESCAPE = $(subst .a ,.a' ',$(subst .o ,.o' ',$(call SHESCAPE,$(1))))

ifeq ($(call MKESCAPE,$(BUILDDIR)),)
    $(error BUILDDIR not specified)
endif

override CFLAGS_FOR_TARGET += \
    -Os \
    -Wall \
    -Wextra \
    -Wshadow \
    -Wvla \
    $(WERROR_FLAG) \
    -std=gnu11 \
    -nostdinc \
    -ffreestanding \
    -ffunction-sections \
    -fdata-sections \
    -fno-stack-protector \
    -fno-stack-check \
    -fomit-frame-pointer \
    -fno-strict-aliasing \
    -fno-lto \
    -fno-PIE \
    -fno-PIC \
    -m32 \
    -march=i686 \
    -mno-80387

override CPPFLAGS_FOR_TARGET := \
    -isystem ../freestanding-headers \
    -I./tinf \
    -I. \
    $(CPPFLAGS_FOR_TARGET) \
    -MMD \
    -MP

override LDFLAGS_FOR_TARGET += \
    -m elf_i386 \
    -nostdlib \
    -z max-page-size=0x1000 \
    -gc-sections \
    -static \
    -T linker.ld

ifeq ($(LD_FOR_TARGET_HAS_NO_PIE),yes)
    override LDFLAGS_FOR_TARGET += -no-pie
endif

override C_FILES := $(shell find . -type f -name '*.c')
override ASM_FILES := $(shell find . -type f -name '*.asm')
override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM_FILES:.asm=.o) $(C_FILES:.c=.o))
override HEADER_DEPS := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.d))

.PHONY: all
all: $(call MKESCAPE,$(BUILDDIR))/decompressor.bin

$(call MKESCAPE,$(BUILDDIR))/decompressor.bin: $(OBJ)
	$(LD_FOR_TARGET) '$(call OBJESCAPE,$^)' $(LDFLAGS_FOR_TARGET) -o '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf'
	$(OBJCOPY_FOR_TARGET) -O binary '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf' '$(call SHESCAPE,$@)'

-include $(HEADER_DEPS)

$(call MKESCAPE,$(BUILDDIR))/%.o: %.c
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	$(CC_FOR_TARGET) $(CFLAGS_FOR_TARGET) $(CPPFLAGS_FOR_TARGET) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.asm
	$(MKDIR_P) "$$(dirname '$(call SHESCAPE,$@)')"
	nasm '$(call SHESCAPE,$<)' -Wall $(WERROR_FLAG) -f elf32 -o '$(call SHESCAPE,$@)'
