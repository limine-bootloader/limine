#include <stdint.h>
#include <stddef.h>
#include <gzip/tinf.h>

__attribute__((noreturn))
void entry(uint8_t *compressed_stage2, size_t stage2_size, uint8_t boot_drive) {
    // The decompressor should decompress compressed_stage2 to address 0x4000.
    uint8_t *dest = (uint8_t *)0x4000;

    tinf_gzip_uncompress(dest, compressed_stage2, stage2_size);

    __attribute__((noreturn))
    void (*stage2)(uint8_t boot_drive) = (void *)dest;

    stage2(boot_drive);
}
