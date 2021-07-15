#ifndef __DRIVERS__GOP_H__
#define __DRIVERS__GOP_H__

#if uefi == 1

#include <stdint.h>
#include <stdbool.h>
#include <efi.h>
#include <lib/fb.h>

bool init_gop(struct fb_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp);

#endif

#endif
