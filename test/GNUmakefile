CC              = cc
CFLAGS          = -O2 -g -Wall -Wextra -Wpedantic
LDFLAGS         =
LD              = ld
QEMU            = qemu-system-x86_64
QEMUFLAGS       = -m 1G -enable-kvm -cpu host

INTERNALLDFLAGS :=         \
	-Tlinker.ld            \
	-nostdlib              \
	-zmax-page-size=0x1000 \
	-static                \
	-pie                   \
	--no-dynamic-linker    \
	-ztext

INTERNAL_LD_FLAGS_MULTIBOOT2 := \
	-Tmultiboot2.ld             \
	-nostdlib                   \
	-zmax-page-size=0x1000      \
	-static

INTERNAL_LD_FLAGS_MULTIBOOT1 := \
	-Tmultiboot.ld              \
	-nostdlib                   \
	-zmax-page-size=0x1000      \
	-static

INTERNALCFLAGS  :=       \
	-I.                  \
	-I..                 \
	-std=c11             \
	-g                   \
	-ffreestanding       \
	-fno-stack-protector \
	-fpie                \
	-mabi=sysv           \
	-mno-80387           \
	-mno-mmx             \
	-mno-3dnow           \
	-mno-sse             \
	-mno-sse2            \
	-mno-red-zone

INTERNALCFLAGS_MB  :=    \
	-I.                  \
	-I..                 \
	-I../common/protos   \
	-std=c11             \
	-m32                 \
	-g                   \
	-ffreestanding       \
	-fno-stack-protector \
	-fno-pie             \
	-fno-pic             \
	-mabi=sysv           \
	-mno-80387           \
	-mno-mmx             \
	-mno-3dnow           \
	-mno-sse             \
	-mno-sse2

all: test.elf multiboot2.elf multiboot.elf

test.elf: limine.o e9print.o memory.o
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
	rm -rf test.elf limine.o e9print.o memory.o
	rm -rf multiboot2.o multiboot2.elf multiboot2_trampoline.o
	rm -rf multiboot.o multiboot_trampoline.o multiboot.elf
