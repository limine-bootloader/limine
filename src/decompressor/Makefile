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
BC := $(C_FILES:.c=.bc)

all: decompressor.bin

decompressor.bin: $(BC)
	llvm-link $(BC) -o bundle.bc
	opt --O$(OPT_LEVEL) bundle.bc -o optimised_bundle.bc
	clang --target=i386-elf -O$(OPT_LEVEL) -c optimised_bundle.bc -o optimised_bundle.o
	ld.lld optimised_bundle.o $(INTERNAL_LDFLAGS) -o decompressor.elf
	llvm-objcopy -O binary decompressor.elf decompressor.bin

%.bc: %.c
	clang --target=i386-elf $(CFLAGS) $(INTERNAL_CFLAGS) -c $< -o $@

clean:
	rm -f decompressor.bin $(BC)
