#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/image.h>
#include <lib/blib.h>
#include <drivers/vbe.h>

void term_vbe(uint32_t *colours, int margin, int margin_gradient, struct image *background) {
    term_deinit();
    if (!vbe_tty_init(&term_rows, &term_cols, colours, margin, margin_gradient, background)) {
        // Failed to set VBE properly, default to text mode
        term_textmode();
        return;
    }

    raw_putchar    = vbe_putchar;
    clear          = vbe_clear;
    enable_cursor  = vbe_enable_cursor;
    disable_cursor = vbe_disable_cursor;
    set_cursor_pos = vbe_set_cursor_pos;
    get_cursor_pos = vbe_get_cursor_pos;
    set_text_fg    = vbe_set_text_fg;
    set_text_bg    = vbe_set_text_bg;

    term_double_buffer       = vbe_double_buffer;
    term_double_buffer_flush = vbe_double_buffer_flush;

    term_backend = VBE;
}
