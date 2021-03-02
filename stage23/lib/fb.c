#include <stdint.h>
#include <stdbool.h>
#include <lib/fb.h>
#include <drivers/vbe.h>
#include <drivers/gop.h>

bool fb_init(struct fb_info *ret,
             uint16_t target_width, uint16_t target_height, uint16_t target_bpp) {
#if defined (bios)
    return init_vbe(ret, target_width, target_height, target_bpp);
#elif defined (uefi)
    return init_gop(ret, target_width, target_height, target_bpp);
#endif
}
