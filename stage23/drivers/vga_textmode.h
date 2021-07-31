#ifndef __DRIVERS__VGA_TEXTMODE_H__
#define __DRIVERS__VGA_TEXTMODE_H__

#include <stdbool.h>

void init_vga_textmode(int *rows, int *cols, bool managed);

void text_putchar(uint8_t c);
void text_clear(bool move);
void text_enable_cursor(void);
bool text_disable_cursor(void);
void text_set_cursor_pos(int x, int y);
void text_get_cursor_pos(int *x, int *y);
void text_set_text_fg(int fg);
void text_set_text_bg(int bg);
void text_set_text_fg_default(void);
void text_set_text_bg_default(void);
bool text_scroll_disable(void);
void text_scroll_enable(void);
void text_move_character(int new_x, int new_y, int old_x, int old_y);

void text_double_buffer(bool state);
void text_double_buffer_flush(void);

#endif
