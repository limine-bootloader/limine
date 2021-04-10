#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/image.h>
#include <lib/blib.h>
#include <lib/gterm.h>

void term_vbe(uint32_t *colours, int margin, int margin_gradient, struct image *background) {
    term_backend = NOT_READY;

    if (!gterm_init(&term_rows, &term_cols, colours, margin, margin_gradient, background)) {
#if defined (bios)
        // Failed to set VBE properly, default to text mode
        term_textmode();
#endif
        return;
    }

    raw_putchar    = gterm_putchar;
    clear          = gterm_clear;
    enable_cursor  = gterm_enable_cursor;
    disable_cursor = gterm_disable_cursor;
    set_cursor_pos = gterm_set_cursor_pos;
    get_cursor_pos = gterm_get_cursor_pos;
    set_text_fg    = gterm_set_text_fg;
    set_text_bg    = gterm_set_text_bg;

    term_double_buffer       = gterm_double_buffer;
    term_double_buffer_flush = gterm_double_buffer_flush;

    term_backend = VBE;
}
