TARGET := test.elf

CC              = cc
CFLAGS          = -O2
LD              = ld
QEMU            = qemu-system-x86_64
QEMUFLAGS       = -m 1G -enable-kvm -cpu host
LDINTERNALFLAGS := -Tlinker.ld -static -nostdlib -no-pie
INTERNALCFLAGS  := -I../stivale -I. -ffreestanding -fno-stack-protector   \
	-fno-pic -fomit-frame-pointer -mno-80387 -mno-mmx -mno-3dnow -mno-sse \
	-mno-sse2 -masm=intel -mcmodel=kernel

all: $(TARGET)

$(TARGET): stivale.o stivale2.o e9print.o
	$(LD) $(LDINTERNALFLAGS) stivale.o stivale2.o e9print.o -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

clean:
	rm -rf $(TARGET) stivale.o stivale2.o e9print.o
