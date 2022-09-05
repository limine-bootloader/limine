include $(CROSS_FILE)

override CC := $(CROSS_CC)
CFLAGS = -O2 -g -Wall -Wextra
LDFLAGS =
override LD := $(CROSS_LD)
override QEMU := qemu-system-x86_64
QEMUFLAGS = -m 1G -enable-kvm -cpu host

override INTERNALLDFLAGS :=         \
	-Tlinker.ld            \
	-nostdlib              \
	-zmax-page-size=0x1000 \
	-static                \
	-pie                   \
	--no-dynamic-linker    \
	-ztext

override INTERNAL_LD_FLAGS_MULTIBOOT2 := \
	-m elf_i386                 \
	-Tmultiboot2.ld             \
	-nostdlib                   \
	-zmax-page-size=0x1000      \
	-static

override INTERNAL_LD_FLAGS_MULTIBOOT1 := \
	-m elf_i386                 \
	-Tmultiboot.ld              \
	-nostdlib                   \
	-zmax-page-size=0x1000      \
	-static

override INTERNALCFLAGS  :=       \
	-std=c11             \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-stack-check     \
	-fno-lto             \
	-fpie                \
	-mgeneral-regs-only  \
	-mno-red-zone        \
	-Istivale            \
	-I.                  \
	-I..

override INTERNALCFLAGS_MB  :=    \
	-std=c11             \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-stack-check     \
	-fno-lto             \
	-fno-pie             \
	-fno-pic             \
	-m32                 \
	-march=i686          \
	-mabi=sysv           \
	-mno-80387           \
	-I.                  \
	-I..                 \
	-I../common/protos

ifneq ($(findstring 86,$(CROSS_CC)),)
all: test.elf multiboot2.elf multiboot.elf
else
all: test.elf
endif

test.elf: stivale.o stivale2.o limine.o e9print.o memory.o
	$(LD) $^ $(LDFLAGS) $(INTERNALLDFLAGS) -o $@

multiboot2.elf: multiboot2_trampoline.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS_MB) -c multiboot2.c -o multiboot2.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS_MB) -c e9print.c -o e9print.o
	$(LD) $^ multiboot2.o e9print.o $(LDFLAGS) $(INTERNAL_LD_FLAGS_MULTIBOOT2) -m elf_i386 -o $@

multiboot.elf: multiboot_trampoline.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS_MB) -c multiboot.c -o multiboot.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS_MB) -c e9print.c -o e9print.o
	$(LD) $^ multiboot.o e9print.o $(LDFLAGS) $(INTERNAL_LD_FLAGS_MULTIBOOT1) -m elf_i386 -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

%.o: %.asm
	nasm -felf32 -F dwarf -g $< -o $@

clean:
	rm -rf test.elf limine.o stivale.o stivale2.o e9print.o memory.o
	rm -rf multiboot2.o multiboot2.elf multiboot2_trampoline.o
	rm -rf multiboot.o multiboot_trampoline.o multiboot.elf
