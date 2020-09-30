#include <stdint.h>
#include <stddef.h>
#include <lib/image.h>
#include <lib/config.h>
#include <lib/blib.h>
#include <mm/pmm.h>
#include <lib/bmp.h>

int open_image(struct image *image, struct file_handle *file) {
    image->file = file;

    if (!bmp_open_image(image, file))
        return 0;

    return -1;
}

struct kernel_loc get_kernel_loc(int boot_drive) {
    int kernel_drive; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "KERNEL_DRIVE")) {
            kernel_drive = boot_drive;
        } else {
            kernel_drive = (int)strtoui(buf);
        }
    }

    int kernel_part; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "KERNEL_PARTITION")) {
            panic("KERNEL_PARTITION not specified");
        } else {
            kernel_part = (int)strtoui(buf);
        }
    }

    char *kernel_path = conv_mem_alloc(128);
    if (!config_get_value(kernel_path, 0, 128, "KERNEL_PATH")) {
        panic("KERNEL_PATH not specified");
    }

    struct file_handle *fd = conv_mem_alloc(sizeof(struct file_handle));
    if (fopen(fd, kernel_drive, kernel_part, kernel_path)) {
        panic("Could not open kernel file");
    }

    return (struct kernel_loc) { kernel_drive, kernel_part, kernel_path, fd };
} 
