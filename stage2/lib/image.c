#include <stdint.h>
#include <stddef.h>
#include <lib/image.h>
#include <lib/bmp.h>

int open_image(struct image *image, struct file_handle *file) {
    image->file = file;

    if (!bmp_open_image(image, file))
        return 0;

    return -1;
}
