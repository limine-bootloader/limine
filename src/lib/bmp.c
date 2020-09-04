#include <drivers/vbe.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <fs/file.h>
#include <lib/bmp.h>

void draw_bmp(struct file_handle fd) {
    bmp_file_hdr_t bmp_file_hdr;
    fread(&fd, &bmp_file_hdr, 0, sizeof(bmp_file_hdr_t));

    uint32_t pitch = bmp_file_hdr.bi_width * (bmp_file_hdr.bi_bpp / 8);

    uint32_t *data = balloc(pitch); 
    uint32_t cnt = bmp_file_hdr.bf_offset;

    for (int i = bmp_file_hdr.bi_height - 1; i > -1; i--) {
        fread(&fd, data, cnt, pitch);
        cnt += pitch;
        for (uint32_t j = 0; j < bmp_file_hdr.bi_width; j++)
            vbe_plot_px(j, i, data[j]);
    }

    brewind(pitch);
}
