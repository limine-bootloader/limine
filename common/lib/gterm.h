#ifndef __LIB__GTERM_H__
#define __LIB__GTERM_H__

#include <stddef.h>
#include <stdbool.h>
#include <lib/fb.h>

extern struct fb_info fbinfo;

bool gterm_init(char *config, size_t width, size_t height);

#endif
