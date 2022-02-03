#ifndef __LIB__BMP_H__
#define __LIB__BMP_H__

#include <stdint.h>
#include <fs/file.h>
#include <lib/image.h>

bool bmp_open_image(struct image *image, struct file_handle *file);

#endif
