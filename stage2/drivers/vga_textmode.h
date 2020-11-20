#ifndef __DRIVERS__VGA_TEXTMODE_H__
#define __DRIVERS__VGA_TEXTMODE_H__

#include <stdbool.h>

void init_vga_textmode(int *rows, int *cols);

void text_putchar(char c);
void text_clear(bool move);
void text_enable_cursor(void);
void text_disable_cursor(void);
void text_set_cursor_pos(int x, int y);
void text_get_cursor_pos(int *x, int *y);
void text_set_text_fg(int fg);
void text_set_text_bg(int bg);

void text_double_buffer(bool state);
void text_double_buffer_flush(void);

#endif
