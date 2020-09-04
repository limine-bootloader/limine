#include <lib/config.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <lib/bmp.h>

void draw_background() {
    char drive[4];
    char partition[4];
    char path[128];

    char *dependences[] = {   "BACKGROUND_IMAGE", (char*)&path,
                              "BACKGROUND_DRIVE", (char*)&drive, 
                              "BACKGROUND_PARTITION", (char*)&partition,
                              "BACKGROUND_PATH", (char*)&path
                          };

    for(int i = 0; i < sizeof(dependences) / sizeof(char*); i += 2) {
        if(!config_get_value(dependences[i + 1], 0, 128, dependences[i])) {
            panic("%s not specified", dependences[i]); 
        }
    }

    print("background drive %s", drive);
    print("background partition %s", partition);
    print("backgroiund path %s", path);
}
