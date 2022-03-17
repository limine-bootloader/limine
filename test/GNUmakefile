CC              = cc
CFLAGS          = -O2 -g
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
	-static                     \
	--no-dynamic-linker         \

INTERNAL_LD_FLAGS_MULTIBOOT1 := \
	-Tmultiboot.ld              \
	-nostdlib                   \
	-zmax-page-size=0x1000      \
	-static                     \
	--no-dynamic-linker         \

INTERNALCFLAGS  :=       \
	-I../stivale         \
	-I.                  \
	-I../common          \
	-std=gnu11           \
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

all: test.elf multiboot2.elf multiboot.elf

test.elf: stivale.o stivale2.o limine.o e9print.o memory.o
	$(LD) $^ $(LDFLAGS) $(INTERNALLDFLAGS) -o $@

multiboot2.elf: multiboot2_trampoline.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -I../common/protos -m32 -c multiboot2.c -o multiboot2.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -m32 -c e9print.c -o e9print.o
	$(LD) $^ multiboot2.o e9print.o $(LDFLAGS) $(INTERNAL_LD_FLAGS_MULTIBOOT2) -m elf_i386 -o $@

multiboot.elf: multiboot_trampoline.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -I../common/protos -m32 -c multiboot.c -o multiboot.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -m32 -c e9print.c -o e9print.o
	$(LD) $^ multiboot.o e9print.o $(LDFLAGS) $(INTERNAL_LD_FLAGS_MULTIBOOT1) -m elf_i386 -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

%.o: %.asm
	nasm -felf32 $< -o $@

clean:
	rm -rf test.elf limine.o stivale.o stivale2.o e9print.o memory.o
	rm -rf multiboot2.o multiboot2.elf multiboot2_trampoline.o
	rm -rf multiboot.o multiboot_trampoline.o multiboot.elf
