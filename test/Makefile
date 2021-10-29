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

all: test.elf

test.elf: stivale.o stivale2.o e9print.o memory.o
	$(LD) $^ $(LDFLAGS) $(INTERNALLDFLAGS) -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(INTERNALCFLAGS) -c $< -o $@

clean:
	rm -rf test.elf stivale.o stivale2.o e9print.o memory.o
