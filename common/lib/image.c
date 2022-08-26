#include <stdint.h>
#include <stddef.h>
#include <lib/config.h>
#include <lib/misc.h>
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

    image->x_size = new_x_size;
    image->y_size = new_y_size;
}

struct image *image_open(struct file_handle *file) {
    struct image *image = ext_mem_alloc(sizeof(struct image));

    image->type = IMAGE_TILED;

    if (bmp_open_image(image, file))
        return image;

    pmm_free(image, sizeof(struct image));
    return NULL;
}

void image_close(struct image *image) {
    pmm_free(image->img, image->allocated_size);
    pmm_free(image, sizeof(struct image));
}
