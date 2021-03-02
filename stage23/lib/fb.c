#include <stdint.h>
#include <stdbool.h>
#include <lib/fb.h>
#include <drivers/vbe.h>

bool fb_init(struct fb_info *ret,
             uint16_t target_width, uint16_t target_height, uint16_t target_bpp) {
    return init_vbe(ret, target_width, target_height, target_bpp);
}
