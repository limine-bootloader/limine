#ifndef __LIB__IMAGE_H__
#define __LIB__IMAGE_H__

#include <stdint.h>
#include <fs/file.h>

struct image {
    struct file_handle *file;
    int x_size;
    int y_size;
    int type;
    union {
        struct {
            int x_displacement;
            int y_displacement;
        };
        struct {
            int old_x_size;
            int old_y_size;
        };
    };
    uint32_t back_colour;
    uint32_t (*get_pixel)(struct image *this, int x, int y);
    void *local;
};

enum {
    IMAGE_TILED,
    IMAGE_CENTERED,
    IMAGE_STRETCHED
};

void image_make_centered(struct image *image, int frame_x_size, int frame_y_size, uint32_t back_colour);
void image_make_stretched(struct image *image, int new_x_size, int new_y_size);
int open_image(struct image *image, struct file_handle *file);

#endif
