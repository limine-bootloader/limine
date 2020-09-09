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

C_FILES := $(shell find ./ -type f -name '*.c' | grep -v bootsect | grep -v decompressor | sort)
ASM_FILES := $(shell find ./ -type f -name '*.asm' | grep -v bootsect | grep -v decompressor | sort)
ASM_OBJ := $(ASM_FILES:.asm=.o)
BC := $(C_FILES:.c=.bc)

all: limine.bin

limine.bin: libgcc.a $(BC) $(ASM_OBJ)
	llvm-link $(BC) -o bundle.bc
	opt --O$(OPT_LEVEL) bundle.bc -o optimised_bundle.bc
	clang --target=i386-elf -O$(OPT_LEVEL) -c optimised_bundle.bc -o optimised_bundle.o
	ld.lld optimised_bundle.o $(ASM_OBJ) $(INTERNAL_LDFLAGS) -o stage2.elf
	llvm-objcopy -O binary stage2.elf stage2.bin
	gzip -9 stage2.bin
	$(MAKE) -C decompressor
	cd bootsect && nasm bootsect.asm -fbin -o ../limine.bin

%.bc: %.c
	clang --target=i386-elf $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

%.o: %.asm
	nasm $< -f elf32 -o $@

clean:
	$(MAKE) -C decompressor clean
	rm -f stage2.bin.gz limine.bin $(ASM_OBJ) $(BC)
