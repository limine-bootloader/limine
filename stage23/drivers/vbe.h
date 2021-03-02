#ifndef __DRIVERS__VBE_H__
#define __DRIVERS__VBE_H__

#include <stdint.h>
#include <stdbool.h>

struct vbe_framebuffer_info {
    uint8_t  memory_model;
    uint32_t framebuffer_addr;
    uint16_t framebuffer_pitch;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
};

bool init_vbe(struct vbe_framebuffer_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp);

#endif
