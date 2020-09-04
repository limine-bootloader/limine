#ifndef IMAGE_H
#define IMAGE_H

typedef struct {
    int part; 
    int drive;
    char *path;
} background_image_info_t;

int get_image_info(background_image_info_t *background_image_info);

void draw_image();

#endif
