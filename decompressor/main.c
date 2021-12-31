#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <tinf.h>

noreturn void entry(uint8_t *compressed_stage2, size_t stage2_size, uint8_t boot_drive, int pxe) {
    // The decompressor should decompress compressed_stage2 to address 0x8000.
    uint8_t *dest = (uint8_t *)0x8000;

    tinf_gzip_uncompress(dest, compressed_stage2, stage2_size);

    asm volatile (
        "movl $0x7c00, %%esp\n\t"
        "xorl %%ebp, %%ebp\n\t"
        "pushl %1\n\t"
        "pushl %0\n\t"
        "pushl $0\n\t"
        "pushl $0x8000\n\t"
        "ret\n\t"
        :
        : "r" ((uint32_t)boot_drive), "r" (pxe)
        : "memory"
    );

    __builtin_unreachable();
}
