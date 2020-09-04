#include <lib/config.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/bmp.h>

const char *file_extensions[] = { ".bmp" };

void (*image_handler[])(background_image_info_t) = { draw_bmp };

int get_image_info(background_image_info_t *image_info) {
    char drive[4];
    char partition[4];
    char path[128];

    char *dependences[] = {   "BACKGROUND_IMAGE", (char*)&path,
                              "BACKGROUND_DRIVE", (char*)&drive, 
                              "BACKGROUND_PARTITION", (char*)&partition,
                              "BACKGROUND_PATH", (char*)&path
                          };

    for(uint32_t i = 0; i < SIZEOF_ARRAY(dependences); i += 2) {
        if(!config_get_value(dependences[i + 1], 0, 128, dependences[i])) {
            print("%s not specified\n", dependences[i]); 
            return 0;
        }
    }
    
    *image_info = (background_image_info_t) { (int)strtoui(drive), (int)strtoui(partition), path };

    return 1;
}

void draw_image() {
    background_image_info_t image_info; 

    if(!get_image_info(&image_info)) 
        return;

    char file_extension[8];

    for(int i = 0; i < SIZEOF_ARRAY(file_extensions); i++) { 
        if(!strcmp(file_extensions[i], image_info.path + (strlen(image_info.path) - strlen(file_extensions[i])))) {
            print("Found extension %s\n", file_extensions[i]);
            image_handler[i](image_info);
        }
    }
} 
