#ifndef BMP_H
#define BMP_H

#include <lib/image.h>

#include <stdint.h>

typedef struct {
    uint16_t bf_type;
    uint32_t bf_size;
    uint32_t reserved;
    uint32_t bf_offset;

    uint32_t bi_size;
    uint32_t bi_width;
    uint32_t bi_height;
    uint32_t bi_planes;
    uint32_t bi_bpp;
    uint32_t bi_compression;
    uint32_t bi_image_size;
    uint32_t bi_xcount;
    uint32_t bi_ycount;
    uint32_t bi_clr_used;
    uint32_t bi_clr_important;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
} __attribute__((packed)) bmp_file_hdr_t;

void draw_bmp(background_image_info_t image_info);

#endif
