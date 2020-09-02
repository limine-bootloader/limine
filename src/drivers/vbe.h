#ifndef __DRIVERS__VBE_H__
#define __DRIVERS__VBE_H__

#include <stdint.h>

struct vbe_char {
    char c;
    uint32_t fg;
    uint32_t bg;
};

int init_vbe(uint32_t **framebuffer, uint16_t *pitch, uint16_t *target_width, uint16_t *target_height, uint16_t *target_bpp);

#endif
