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

struct bmp_local {
    uint8_t  *image;
    uint32_t  pitch;
    struct bmp_header header;
};

stage3_text
static uint32_t get_pixel(struct image *this, int x, int y) {
    struct bmp_local *local = this->local;
    struct bmp_header *header = &local->header;

    x %= header->bi_width;
    y %= header->bi_height;

    size_t pixel_offset = local->pitch * (header->bi_height - y - 1) + x * (header->bi_bpp / 8);

    // TODO: Perhaps use masks here, they're there for a reason
    uint32_t composite = 0;
    for (int i = 0; i < header->bi_bpp / 8; i++)
        composite |= (uint32_t)local->image[pixel_offset + i] << (i * 8);

    return composite;
}

stage3_text
int bmp_open_image(struct image *image, struct file_handle *file) {
    struct bmp_header header;
    fread(file, &header, 0, sizeof(struct bmp_header));

    if (memcmp(&header.bf_signature, "BM", 2) != 0)
        return -1;

    // We don't support bpp lower than 8
    if (header.bi_bpp < 8)
        return -1;

    struct bmp_local *local = ext_mem_alloc(sizeof(struct bmp_local));

    local->image = ext_mem_alloc(header.bf_size);
    fread(file, local->image, header.bf_offset, header.bf_size);

    local->pitch  = ALIGN_UP(header.bi_width * header.bi_bpp, 32) / 8;
    local->header = header;

    image->x_size    = header.bi_width;
    image->y_size    = header.bi_height;
    image->get_pixel = get_pixel;
    image->local     = local;

    return 0;
}
