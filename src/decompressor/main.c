asm (
    ".section .entry\n\t"

    // Zero out .bss
    "xor al, al\n\t"
    "lea edi, bss_begin\n\t"
    "lea ecx, bss_end\n\t"
    "lea edx, bss_begin\n\t"
    "sub ecx, edx\n\t"
    "rep stosb\n\t"

    "jmp main\n\t"
);

#include <stdint.h>
#include <stddef.h>

__attribute__((noreturn))
void main(uint8_t *compressed_stage2, size_t stage2_size, uint8_t boot_drive) {
    // The decompressor should decompress compressed_stage2 to address 0x500.
    // For now, just copy it over as it is not compressed. TODO: implement decompressor.
    volatile uint8_t *dest = (volatile uint8_t *)0x500;

    for (size_t i = 0; i < stage2_size; i++)
        dest[i] = compressed_stage2[i];

    __attribute__((noreturn))
    void (*stage2)(uint8_t boot_drive) = (void *)dest;

    stage2(boot_drive);
}
