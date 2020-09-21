#ifndef __LIB__IMAGE_H__
#define __LIB__IMAGE_H__

#include <stdint.h>
#include <fs/file.h>

struct image {
    struct file_handle *file;
    int x_size;
    int y_size;
    uint32_t (*get_pixel)(struct image *this, int x, int y);
    void *local;
};

int open_image(struct image *image, struct file_handle *file);

#endif
