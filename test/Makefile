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

INTERNALLDFLAGSMBOOT2 :=   \
	-Tmultiboot2.ld        \
	-nostdlib              \
	-zmax-page-size=0x1000 \
	-static                \
	--no-dynamic-linker    \

INTERNALCFLAGS  :=       \
	-I../stivale         \
	-I.                  \
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

all: test.elf multiboot2.elf

test.elf: stivale.o stivale2.o e9print.o memory.o
	$(LD) $^ $(LDFLAGS) $(INTERNALLDFLAGS) -o $@

multiboot2.elf: multiboot2_trampoline.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -I../stage23/protos -m32 -c multiboot2.c -o multiboot2.o
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -m32 -c e9print.c -o e9print.o
	$(LD) $^ multiboot2.o e9print.o $(LDFLAGS) $(INTERNALLDFLAGSMBOOT2) -m elf_i386 -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

%.o: %.asm
	nasm -felf32 $< -o $@

clean:
	rm -rf test.elf stivale.o stivale2.o e9print.o memory.o multiboot2.o multiboot2.elf multiboot2_trampoline.o
