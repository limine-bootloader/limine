#ifndef __DRIVERS__VGA_TEXTMODE_H__
#define __DRIVERS__VGA_TEXTMODE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

void init_vga_textmode(size_t *rows, size_t *cols, bool managed);

void text_putchar(uint8_t c);
void text_clear(bool move);
void text_enable_cursor(void);
bool text_disable_cursor(void);
void text_set_cursor_pos(size_t x, size_t y);
void text_get_cursor_pos(size_t *x, size_t *y);
void text_set_text_fg(size_t fg);
void text_set_text_bg(size_t bg);
void text_set_text_fg_bright(size_t fg);
void text_set_text_fg_default(void);
void text_set_text_bg_default(void);
bool text_scroll_disable(void);
void text_scroll_enable(void);
void text_move_character(size_t new_x, size_t new_y, size_t old_x, size_t old_y);
void text_scroll(void);
void text_swap_palette(void);

void text_double_buffer(bool state);
void text_double_buffer_flush(void);

uint64_t text_context_size(void);
void text_context_save(uint64_t ptr);
void text_context_restore(uint64_t ptr);
void text_full_refresh(void);

#endif
