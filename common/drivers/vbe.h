#ifndef __DRIVERS__VBE_H__
#define __DRIVERS__VBE_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/fb.h>

bool init_vbe(struct fb_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp);

#endif
