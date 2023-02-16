#ifndef __LIB__GTERM_H__
#define __LIB__GTERM_H__

#include <stddef.h>
#include <stdbool.h>
#include <lib/fb.h>

bool gterm_init(struct fb_info **ret, size_t *_fbs_count,
                char *config, size_t width, size_t height);

#endif
