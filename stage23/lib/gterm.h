#ifndef __LIB__GTERM_H__
#define __LIB__GTERM_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/image.h>
#include <drivers/vbe.h>

extern struct fb_info fbinfo;

bool gterm_init(size_t *_rows, size_t *_cols, size_t width, size_t height);
void gterm_deinit(void);

void gterm_putchar(uint8_t c);
void gterm_clear(bool move);
void gterm_enable_cursor(void);
bool gterm_disable_cursor(void);
void gterm_set_cursor_pos(size_t x, size_t y);
void gterm_get_cursor_pos(size_t *x, size_t *y);
void gterm_set_text_fg(size_t fg);
void gterm_set_text_bg(size_t bg);
void gterm_set_text_fg_bright(size_t fg);
void gterm_set_text_bg_bright(size_t bg);
void gterm_set_text_fg_default(void);
void gterm_set_text_bg_default(void);
bool gterm_scroll_disable(void);
void gterm_scroll_enable(void);
void gterm_move_character(size_t new_x, size_t new_y, size_t old_x, size_t old_y);
void gterm_scroll(void);
void gterm_swap_palette(void);

void gterm_double_buffer_flush(void);

uint64_t gterm_context_size(void);
void gterm_context_save(uint64_t ptr);
void gterm_context_restore(uint64_t ptr);
void gterm_full_refresh(void);

#endif
