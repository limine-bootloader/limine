CC              = cc
CFLAGS          = -O2
LD              = cc
QEMU            = qemu-system-x86_64
QEMUFLAGS       = -m 1G -enable-kvm -cpu host
LDINTERNALFLAGS := -Tlinker.ld -nostdlib -pie -static-pie -fno-pic -fpie -z max-page-size=0x1000
INTERNALCFLAGS  := -I../stivale -I. -ffreestanding -fno-stack-protector   \
	-fno-pic -fpie -fomit-frame-pointer -mno-red-zone -mgeneral-regs-only -masm=intel

all: test.elf

test.elf: stivale.o stivale2.o e9print.o
	$(LD) $(LDINTERNALFLAGS) stivale.o stivale2.o e9print.o -o test.elf

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

clean:
	rm -rf test.elf stivale.o stivale2.o e9print.o
