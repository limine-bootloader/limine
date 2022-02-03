#ifndef __LIB__FB_H__
#define __LIB__FB_H__

#include <stdint.h>

struct resolution {
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
};

struct fb_info {
    bool default_res;
    uint8_t  memory_model;
    uint64_t framebuffer_addr;
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

bool fb_init(struct fb_info *ret,
             uint16_t target_width, uint16_t target_height, uint16_t target_bpp);

void fb_clear(struct fb_info *fb);

#endif
