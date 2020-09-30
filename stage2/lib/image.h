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

struct kernel_loc {
    int kernel_drive; 
    int kernel_part;
    char *kernel_path;
    struct file_handle *fd;
};

int open_image(struct image *image, struct file_handle *file);

struct kernel_loc get_kernel_loc(int boot_drive);

#endif
