CC              = cc
CFLAGS          = -O2
LD              = cc
QEMU            = qemu-system-x86_64
QEMUFLAGS       = -m 1G -enable-kvm -cpu host
LDINTERNALFLAGS := -Tlinker.ld -nostdlib -fno-pic -fpie -z max-page-size=0x1000 \
	-Wl,-static,-pie,--no-dynamic-linker,-ztext -static-pie
INTERNALCFLAGS  := -I../stivale -I. -ffreestanding -fno-stack-protector \
	-fno-pic -fpie -mno-red-zone -mno-80387 -mno-mmx -mno-3dnow -mno-sse -mno-sse2

all: test.elf

test.elf: stivale.o stivale2.o e9print.o memory.o
	$(LD) $(LDINTERNALFLAGS) $^ -o test.elf

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

clean:
	rm -rf test.elf stivale.o stivale2.o e9print.o memory.o
