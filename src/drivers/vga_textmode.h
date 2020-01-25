#ifndef __VGA_TEXTMODE_H__
#define __VGA_TEXTMODE_H__

#include <stddef.h>

void init_vga_textmode(void);
void text_write(const char *, size_t);

void text_get_cursor_pos(int *x, int *y);
void text_set_cursor_pos(int x, int y);

#endif
