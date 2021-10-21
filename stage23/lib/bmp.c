#include <stdint.h>
#include <fs/file.h>
#include <lib/image.h>
#include <lib/bmp.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <mm/pmm.h>

struct bmp_header {
    uint16_t bf_signature;
    uint32_t bf_size;
    uint32_t reserved;
    uint32_t bf_offset;

    uint32_t bi_size;
    uint32_t bi_width;
    uint32_t bi_height;
    uint16_t bi_planes;
    uint16_t bi_bpp;
    uint32_t bi_compression;
    uint32_t bi_image_size;
    uint32_t bi_xcount;
    uint32_t bi_ycount;
    uint32_t bi_clr_used;
    uint32_t bi_clr_important;
    uint32_t red_mask;
    uint32_t green_mask;
    uint32_t blue_mask;
} __attribute__((packed));

bool bmp_open_image(struct image *image, struct file_handle *file) {
    struct bmp_header header;
    fread(file, &header, 0, sizeof(struct bmp_header));

    if (memcmp(&header.bf_signature, "BM", 2) != 0)
        return false;

    // We don't support bpp lower than 8
    if (header.bi_bpp % 8 != 0)
        return false;

    image->img = ext_mem_alloc(header.bf_size);

    uint32_t bf_size;
    if (header.bf_offset + header.bf_size > file->size) {
        bf_size = file->size - header.bf_offset;
    } else {
        bf_size = header.bf_size;
    }

    fread(file, image->img, header.bf_offset, bf_size);

    image->allocated_size = header.bf_size;

    image->x_size     = header.bi_width;
    image->y_size     = header.bi_height;
    image->pitch      = ALIGN_UP(header.bi_width * header.bi_bpp, 32) / 8;
    image->bpp        = header.bi_bpp;
    image->img_width  = header.bi_width;
    image->img_height = header.bi_height;

    return true;
}
