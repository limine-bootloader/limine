#ifndef __LIB__GTERM_H__
#define __LIB__GTERM_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/image.h>
#include <drivers/vbe.h>

bool gterm_init(int *rows, int *cols, uint32_t *colours, int margin,
                int margin_gradient, struct image *background);

void gterm_putchar(uint8_t c);
void gterm_clear(bool move);
void gterm_enable_cursor(void);
void gterm_disable_cursor(void);
void gterm_set_cursor_pos(int x, int y);
void gterm_get_cursor_pos(int *x, int *y);
void gterm_set_text_fg(int fg);
void gterm_set_text_bg(int bg);

void gterm_double_buffer_flush(void);
void gterm_double_buffer(bool state);

#endif
