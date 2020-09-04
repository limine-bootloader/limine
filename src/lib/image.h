#ifndef IMAGE_H
#define IMAGE_H

#include <stdint.h> 

typedef struct {
    int part; 
    int drive;
    char path[128];
} background_image_info_t;

int get_image_info(background_image_info_t *background_image_info);

void draw_image();

uint32_t get_pixel(int x, int y, uint32_t pitch, uint32_t *data);

#endif
