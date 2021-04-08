#ifndef __LIB__IMAGE_H__
#define __LIB__IMAGE_H__

#include <stdint.h>
#include <fs/file.h>

struct image {
    struct file_handle *file;
    int x_size;
    int y_size;
    int type;
    int x_displacement;
    int y_displacement;
    uint32_t back_colour;
    uint32_t (*get_pixel)(struct image *this, int x, int y);
    void *local;
};

enum {
    IMAGE_TILED,
    IMAGE_CENTERED
};

void image_make_centered(struct image *image, int frame_x_size, int frame_y_size, uint32_t back_colour);
int open_image(struct image *image, struct file_handle *file);

#endif
