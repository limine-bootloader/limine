#ifndef __DRIVERS__VBE_H__
#define __DRIVERS__VBE_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/image.h>

struct vbe_framebuffer_info {
    uint8_t  memory_model;
    uint32_t framebuffer_addr;
    uint16_t framebuffer_pitch;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
    uint8_t  red_mask_size;
    uint8_t  red_mask_shift;
    uint8_t  green_mask_size;
    uint8_t  green_mask_shift;
    uint8_t  blue_mask_size;
    uint8_t  blue_mask_shift;
};

bool init_vbe(struct vbe_framebuffer_info *ret,
              uint16_t target_width, uint16_t target_height, uint16_t target_bpp);

void vbe_tty_init(int *rows, int *cols, uint32_t *colours, int margin, int margin_gradient, struct image *background);

void vbe_putchar(char c);
void vbe_clear(bool move);
void vbe_enable_cursor(void);
void vbe_disable_cursor(void);
void vbe_set_cursor_pos(int x, int y);
void vbe_get_cursor_pos(int *x, int *y);
void vbe_set_text_fg(int fg);
void vbe_set_text_bg(int bg);

#endif
