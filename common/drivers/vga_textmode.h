#ifndef __DRIVERS__VGA_TEXTMODE_H__
#define __DRIVERS__VGA_TEXTMODE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <term/term.h>

struct textmode_context {
    struct term_context term;

    volatile uint8_t *video_mem;

    uint8_t *back_buffer;
    uint8_t *front_buffer;

    size_t cursor_offset;
    size_t old_cursor_offset;
    bool cursor_status;
    uint8_t text_palette;

    uint8_t saved_state_text_palette;
    size_t saved_state_cursor_offset;
};

void init_vga_textmode(size_t *rows, size_t *cols, bool managed);

void text_putchar(struct term_context *ctx, uint8_t c);
void text_clear(struct term_context *ctx, bool move);
void text_enable_cursor(struct term_context *ctx);
bool text_disable_cursor(struct term_context *ctx);
void text_set_cursor_pos(struct term_context *ctx, size_t x, size_t y);
void text_get_cursor_pos(struct term_context *ctx, size_t *x, size_t *y);
void text_set_text_fg(struct term_context *ctx, size_t fg);
void text_set_text_bg(struct term_context *ctx, size_t bg);
void text_set_text_fg_bright(struct term_context *ctx, size_t fg);
void text_set_text_bg_bright(struct term_context *ctx, size_t bg);
void text_set_text_fg_default(struct term_context *ctx);
void text_set_text_bg_default(struct term_context *ctx);
bool text_scroll_disable(struct term_context *ctx);
void text_scroll_enable(struct term_context *ctx);
void text_move_character(struct term_context *ctx, size_t new_x, size_t new_y, size_t old_x, size_t old_y);
void text_scroll(struct term_context *ctx);
void text_revscroll(struct term_context *ctx);
void text_swap_palette(struct term_context *ctx);
void text_save_state(struct term_context *ctx);
void text_restore_state(struct term_context *ctx);
void text_double_buffer_flush(struct term_context *ctx);
void text_full_refresh(struct term_context *ctx);

#endif
