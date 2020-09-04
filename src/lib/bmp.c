#include <drivers/vbe.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <fs/file.h>
#include <lib/bmp.h>

uint32_t *bmp_image;

static uint32_t bmp_get_pixel(int x, int y, uint32_t pitch) {
    return bmp_image[x + (pitch / sizeof(uint32_t)) * y];
}

void draw_bmp(struct file_handle fd) {
    bmp_file_hdr_t bmp_file_hdr;
    fread(&fd, &bmp_file_hdr, 0, sizeof(bmp_file_hdr_t));

    bmp_image = (uint32_t*)ext_mem_balloc(bmp_file_hdr.bf_size);
    fread(&fd, bmp_image, bmp_file_hdr.bf_offset, bmp_file_hdr.bf_size);

    uint32_t pitch = bmp_file_hdr.bi_width * (bmp_file_hdr.bi_bpp / 8);

    uint32_t x = 0, y = bmp_file_hdr.bi_height;

    for (uint32_t i = 0; i < bmp_file_hdr.bi_height; i++) {
        for (uint32_t j = 0; j < bmp_file_hdr.bi_width; j++) {
            vbe_plot_px(x++, y, bmp_get_pixel(j, i, pitch));
        }
        x = 0;
        y--;
    }
}
