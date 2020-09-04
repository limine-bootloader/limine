#include <lib/config.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/bmp.h>

const char *file_extensions[] = { ".bmp" };

void (*image_handler[])(struct file_handle) = { draw_bmp };

int get_image_info(background_image_info_t *image_info) {
    char drive[4];
    char partition[4];
    char path[128];

    char *dependences[] = {   "BACKGROUND_IMAGE", (char*)&path,
                              "BACKGROUND_DRIVE", (char*)&drive, 
                              "BACKGROUND_PARTITION", (char*)&partition,
                              "BACKGROUND_PATH", (char*)&path
                          };

    for (uint32_t i = 0; i < SIZEOF_ARRAY(dependences); i += 2) {
        if (!config_get_value(dependences[i + 1], 0, 128, dependences[i])) {
            print("%s not specified\n", dependences[i]); 
            return 0;
        }
    }
    
    background_image_info_t background_image = { (int)strtoui(partition), (int)strtoui(drive) };
    strcpy(background_image.path, path);

    *image_info = background_image;

    return 1;
}

void draw_image() {
    background_image_info_t image_info; 

    if (!get_image_info(&image_info)) 
        return;

    for (uint32_t i = 0; i < SIZEOF_ARRAY(file_extensions); i++) { 
        if (!strcmp(file_extensions[i], image_info.path + (strlen(image_info.path) - strlen(file_extensions[i])))) {
            struct file_handle fd;

            if (fopen(&fd, image_info.drive, image_info.part, image_info.path)) {
                print("%s could not be opened", image_info.path);
                return;
            }

            image_handler[i](fd);
        }
    }
}
