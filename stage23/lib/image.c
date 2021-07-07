#include <stdint.h>
#include <stddef.h>
#include <lib/config.h>
#include <lib/blib.h>
#include <mm/pmm.h>
#include <lib/bmp.h>

void image_make_centered(struct image *image, int frame_x_size, int frame_y_size, uint32_t back_colour) {
    image->type = IMAGE_CENTERED;

    image->x_displacement = frame_x_size / 2 - image->x_size / 2;
    image->y_displacement = frame_y_size / 2 - image->y_size / 2;
    image->back_colour = back_colour;
}


void image_make_stretched(struct image *image, int new_x_size, int new_y_size) {
    image->type = IMAGE_STRETCHED;

    image->old_x_size = image->x_size;
    image->old_y_size = image->y_size;

    image->x_size = new_x_size;
    image->y_size = new_y_size;
}

int open_image(struct image *image, struct file_handle *file) {
    image->file = file;

    if (!bmp_open_image(image, file))
        return 0;

    image->type = IMAGE_TILED;

    return -1;
}
