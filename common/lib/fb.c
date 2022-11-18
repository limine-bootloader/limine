#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/fb.h>
#include <drivers/vbe.h>
#include <drivers/gop.h>
#include <mm/pmm.h>

void fb_init(struct fb_info **ret, size_t *_fbs_count,
             uint64_t target_width, uint64_t target_height, uint16_t target_bpp) {
#if defined (BIOS)
    *ret = ext_mem_alloc(sizeof(struct fb_info));
    if (init_vbe(*ret, target_width, target_height, target_bpp)) {
        *_fbs_count = 1;

        (*ret)->edid = get_edid_info();
        size_t mode_count;
        (*ret)->mode_list = vbe_get_mode_list(&mode_count);
        (*ret)->mode_count = mode_count;
    } else {
        *_fbs_count = 0;
        pmm_free(*ret, sizeof(struct fb_info));
    }
#elif defined (UEFI)
    init_gop(ret, _fbs_count, target_width, target_height, target_bpp);
#endif
}

void fb_clear(struct fb_info *fb) {
    for (size_t y = 0; y < fb->framebuffer_height; y++) {
        switch (fb->framebuffer_bpp) {
            case 32: {
                uint32_t *fbp = (void *)(uintptr_t)fb->framebuffer_addr;
                size_t row = (y * fb->framebuffer_pitch) / 4;
                for (size_t x = 0; x < fb->framebuffer_width; x++) {
                    fbp[row + x] = 0;
                }
                break;
            }
            case 16: {
                uint16_t *fbp = (void *)(uintptr_t)fb->framebuffer_addr;
                size_t row = (y * fb->framebuffer_pitch) / 2;
                for (size_t x = 0; x < fb->framebuffer_width; x++) {
                    fbp[row + x] = 0;
                }
                break;
            }
            default: {
                uint8_t *fbp = (void *)(uintptr_t)fb->framebuffer_addr;
                size_t row = y * fb->framebuffer_pitch;
                for (size_t x = 0; x < fb->framebuffer_width * fb->framebuffer_bpp; x++) {
                    fbp[row + x] = 0;
                }
                break;
            }
        }
    }
}
