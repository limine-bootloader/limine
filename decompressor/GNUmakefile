BUILDDIR ?=
USING_CLANG ?= 0

override SPACE := $(subst ,, )

MKESCAPE = $(subst $(SPACE),\ ,$(1))
SHESCAPE = $(subst ','\'',$(1))
OBJESCAPE = $(subst .a ,.a' ',$(subst .o ,.o' ',$(call SHESCAPE,$(1))))

ifeq ($(call MKESCAPE,$(BUILDDIR)), )
	$(error BUILDDIR not specified)
endif

ifeq ($(USING_CLANG), 1)
override LIMINE_CC += --target=i686-elf
endif

override INTERNAL_CFLAGS := \
	$(WERROR) \
	-m32 \
	-march=i686 \
	-mtune=generic \
	-mabi=sysv \
	-std=gnu11 \
	-ffreestanding \
	-fno-stack-protector \
	-fno-stack-check \
	-fno-strict-aliasing \
	-fno-lto \
	-fno-pic \
	-fno-pie \
	-fomit-frame-pointer \
	-nostdinc \
	-Wno-address-of-packed-member \
	-Wshadow \
	-Wvla \
	-mno-80387 \
	-mno-mmx \
	-mno-3dnow \
	-mno-sse \
	-mno-sse2 \
	-MMD \
	-I../freestanding_headers \
	-I. \
	-I'$(call SHESCAPE,$(BUILDDIR))/tinf'

override INTERNAL_LDFLAGS := \
	-melf_i386 \
	-nostdlib \
	-z max-page-size=0x1000 \
	-static \
	-Tlinker.ld

override C_FILES := $(shell find ./ -type f -name '*.c')
override ASM_FILES := $(shell find ./ -type f -name '*.S')
override OBJ := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(ASM_FILES:.S=.o) $(C_FILES:.c=.o))
override HEADER_DEPS := $(addprefix $(call MKESCAPE,$(BUILDDIR))/, $(C_FILES:.c=.d))

.PHONY: all
all: $(call MKESCAPE,$(BUILDDIR))/decompressor.bin

$(call MKESCAPE,$(BUILDDIR))/decompressor.bin: $(OBJ) $(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o $(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o
	$(LIMINE_LD) '$(call OBJESCAPE,$^)' $(LIMINE_LDFLAGS) $(INTERNAL_LDFLAGS) -o '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf'
	$(LIMINE_OBJCOPY) -O binary '$(call SHESCAPE,$(BUILDDIR))/decompressor.elf' '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/tinf-copied: ../tinf/*
	rm -rf '$(call SHESCAPE,$(BUILDDIR))/tinf'
	mkdir -p '$(call SHESCAPE,$(BUILDDIR))'
	cp -r ../tinf '$(call SHESCAPE,$(BUILDDIR))/'
	touch '$(call SHESCAPE,$(BUILDDIR))/tinf-copied'

$(call MKESCAPE,$(BUILDDIR))/tinf/tinfgzip.o: $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$(@:.o=.c))' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/tinf/tinflate.o: $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$(@:.o=.c))' -o '$(call SHESCAPE,$@)'

-include $(HEADER_DEPS)

$(call MKESCAPE,$(BUILDDIR))/%.o: %.c $(call MKESCAPE,$(BUILDDIR))/tinf-copied
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

$(call MKESCAPE,$(BUILDDIR))/%.o: %.S
	mkdir -p "$$(dirname '$(call SHESCAPE,$@)')"
	$(LIMINE_CC) $(LIMINE_CFLAGS) -Os $(INTERNAL_CFLAGS) -c '$(call SHESCAPE,$<)' -o '$(call SHESCAPE,$@)'

.PHONY: clean
clean:
	rm -rf '$(call SHESCAPE,$(BUILDDIR))'
