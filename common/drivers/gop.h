#ifndef __DRIVERS__GOP_H__
#define __DRIVERS__GOP_H__

#if defined (UEFI)

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <efi.h>
#include <lib/fb.h>

bool init_gop(struct fb_info *ret,
              uint64_t target_width, uint64_t target_height, uint16_t target_bpp);

struct fb_info *gop_get_mode_list(size_t *count);

extern bool gop_force_16;

#endif

#endif
