OPT_LEVEL = z
CFLAGS = -pipe -Wall -Wextra

INTERNAL_CFLAGS = \
	-O$(OPT_LEVEL) \
	-std=gnu99 \
	-ffreestanding \
	-flto \
	-mno-80387 \
	-mno-mmx \
	-mno-sse \
	-mno-sse2 \
	-I. \
	-Wno-address-of-packed-member

INTERNAL_LDFLAGS = \
	-static \
	-nostdlib \
	-Tlinker.ld \
	-no-pie

.PHONY: all clean

C_FILES := $(shell find ./ -type f -name '*.c' | sort)
ASM_FILES := $(shell find ./ -type f -name '*.asm' | grep -v bootsect | sort)
ASM_OBJ := $(ASM_FILES:.asm=.o)
BC := $(C_FILES:.c=.bc)

all: limine.bin

limine.bin: libgcc.a $(BC) $(ASM_OBJ)
	llvm-link $(BC) -o bundle.bc
	opt --O$(OPT_LEVEL) bundle.bc -o optimised_bundle.bc
	clang --target=i386-elf -O$(OPT_LEVEL) -c optimised_bundle.bc -o optimised_bundle.o
	ld.lld optimised_bundle.o $(ASM_OBJ) libgcc.a $(INTERNAL_LDFLAGS) -o stage2.elf
	llvm-objcopy -O binary stage2.elf stage2.bin
	cd bootsect && nasm bootsect.asm -fbin -o ../limine.bin

libgcc.a:
	curl https://mirror.netcologne.de/archlinux/core/os/x86_64/gcc-10.2.0-2-x86_64.pkg.tar.zst | \
		tar -I zstd -xv --strip-components=6 --occurrence=1 usr/lib/gcc/x86_64-pc-linux-gnu/10.2.0/32/libgcc.a

%.bc: %.c
	clang --target=i386-elf $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	rm -f limine.bin $(ASM_OBJ) $(BC)
