#include <lib/asm.h>

ASM_BASIC(
    ".section .entry\n\t"

    "cld\n\t"

    // Zero out .bss
    "xor al, al\n\t"
    "mov edi, OFFSET bss_begin\n\t"
    "mov ecx, OFFSET bss_end\n\t"
    "sub ecx, OFFSET bss_begin\n\t"
    "rep stosb\n\t"

    "mov ebx, OFFSET main\n\t"
    "jmp ebx\n\t"
);

#include <stdint.h>
#include <stddef.h>
#include <gzip/tinf.h>

__attribute__((noreturn))
void main(uint8_t *compressed_stage2, size_t stage2_size, uint8_t boot_drive) {
    // The decompressor should decompress compressed_stage2 to address 0x500.
    // For now, just copy it over as it is not compressed. TODO: implement decompressor.
    volatile uint8_t *dest = (volatile uint8_t *)0x500;

	tinf_gzip_uncompress(dest, compressed_stage2, stage2_size);

    __attribute__((noreturn))
    void (*stage2)(uint8_t boot_drive) = (void *)dest;

    stage2(boot_drive);
}
