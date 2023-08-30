include $(TOOLCHAIN_FILE)
export CC_FOR_TARGET
export LD_FOR_TARGET
export OBJDUMP_FOR_TARGET
export OBJCOPY_FOR_TARGET
export READELF_FOR_TARGET

override CC := $(CC_FOR_TARGET)
CFLAGS = -O2 -g -Wall -Wextra
LDFLAGS =
override LD := $(LD_FOR_TARGET)
override QEMU := qemu-system-x86_64
QEMUFLAGS = -m 1G -enable-kvm -cpu host

ifneq ($(findstring x86_64,$(CC_FOR_TARGET)),)
override LDFLAGS += \
    -m elf_x86_64
endif
ifneq ($(findstring aarch64,$(CC_FOR_TARGET)),)
override LDFLAGS += \
    -m aarch64elf
endif

override LDFLAGS += \
    -Tlinker.ld \
    -nostdlib \
    -zmax-page-size=0x1000 \
    -static \
    -pie \
    --no-dynamic-linker \
    -ztext

override LDFLAGS_MB2 := \
    -m elf_i386 \
    -Tmultiboot2.ld \
    -nostdlib \
    -zmax-page-size=0x1000 \
    -static

override LDFLAGS_MB1 := \
    -m elf_i386 \
    -Tmultiboot.ld \
    -nostdlib \
    -zmax-page-size=0x1000 \
    -static

override CFLAGS += \
    -std=c11 \
    -nostdinc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fPIE \
    -I../freestanding-headers \
    -I. \
    -D_LIMINE_PROTO

ifneq ($(findstring x86_64,$(CC_FOR_TARGET)),)
override CFLAGS += \
    -m64 \
    -march=x86-64 \
    -mgeneral-regs-only \
    -mno-red-zone
endif

ifneq ($(findstring aarch64,$(CC_FOR_TARGET)),)
override CFLAGS += \
    -mgeneral-regs-only
endif

override CFLAGS_MB := \
    -std=c11 \
    -nostdinc \
    -ffreestanding \
    -fno-stack-protector \
    -fno-stack-check \
    -fno-lto \
    -fno-PIE \
    -fno-PIC \
    -m32 \
    -march=i686 \
    -mgeneral-regs-only \
    -I../freestanding-headers \
    -I. \
    -I../common/protos

ifneq ($(findstring 86,$(CC_FOR_TARGET)),)
all: test.elf multiboot2.elf multiboot.elf
else
all: test.elf
endif

limine.h:
	cp -v ../limine.h ./

test.elf: limine.o e9print.o memory.o
	$(LD) $^ $(LDFLAGS) -o $@

multiboot2.elf: multiboot2_trampoline.o
	$(CC) $(CFLAGS_MB) -c memory.c -o memory.o
	$(CC) $(CFLAGS_MB) -c multiboot2.c -o multiboot2.o
	$(CC) $(CFLAGS_MB) -c e9print.c -o e9print.o
	$(LD) $^ memory.o multiboot2.o e9print.o $(LDFLAGS_MB2) -o $@

multiboot.elf: multiboot_trampoline.o
	$(CC) $(CFLAGS_MB) -c memory.c -o memory.o
	$(CC) $(CFLAGS_MB) -c multiboot.c -o multiboot.o
	$(CC) $(CFLAGS_MB) -c e9print.c -o e9print.o
	$(LD) $^ memory.o multiboot.o e9print.o $(LDFLAGS_MB1) -o $@

%.o: %.c limine.h
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.asm
	nasm -felf32 -F dwarf -g $< -o $@

clean:
	rm -rf test.elf limine.o e9print.o memory.o
	rm -rf multiboot2.o multiboot2.elf multiboot2_trampoline.o
	rm -rf multiboot.o multiboot_trampoline.o multiboot.elf
	rm -rf limine.h
