#ifndef __LIB__GTERM_H__
#define __LIB__GTERM_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/image.h>
#include <drivers/vbe.h>

extern struct fb_info fbinfo;

bool gterm_init(int *_rows, int *_cols, int width, int height);

void gterm_putchar(uint8_t c);
void gterm_clear(bool move);
void gterm_enable_cursor(void);
bool gterm_disable_cursor(void);
void gterm_set_cursor_pos(int x, int y);
void gterm_get_cursor_pos(int *x, int *y);
void gterm_set_text_fg(int fg);
void gterm_set_text_bg(int bg);
bool gterm_scroll_disable(void);
void gterm_scroll_enable(void);
void gterm_move_character(int new_x, int new_y, int old_x, int old_y);

void gterm_double_buffer_flush(void);
void gterm_double_buffer(bool state);

#endif
