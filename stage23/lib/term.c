#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/image.h>
#include <lib/blib.h>
#include <lib/gterm.h>

bool early_term = false;

void term_vbe(size_t width, size_t height) {
    term_backend = NOT_READY;

    if (!gterm_init(&term_rows, &term_cols, width, height)) {
#if bios == 1
        // Failed to set VBE properly, default to text mode
        term_textmode();
#endif
        return;
    }

    term_reinit();

    raw_putchar    = gterm_putchar;
    clear          = gterm_clear;
    enable_cursor  = gterm_enable_cursor;
    disable_cursor = gterm_disable_cursor;
    set_cursor_pos = gterm_set_cursor_pos;
    get_cursor_pos = gterm_get_cursor_pos;
    set_text_fg    = gterm_set_text_fg;
    set_text_bg    = gterm_set_text_bg;
    set_text_fg_bright = gterm_set_text_fg_bright;
    set_text_bg_bright = gterm_set_text_bg_bright;
    set_text_fg_default = gterm_set_text_fg_default;
    set_text_bg_default = gterm_set_text_bg_default;
    scroll_disable = gterm_scroll_disable;
    scroll_enable  = gterm_scroll_enable;
    term_move_character = gterm_move_character;
    term_scroll = gterm_scroll;
    term_swap_palette = gterm_swap_palette;

    term_double_buffer       = gterm_double_buffer;
    term_double_buffer_flush = gterm_double_buffer_flush;

    term_context_size = gterm_context_size;
    term_context_save = gterm_context_save;
    term_context_restore = gterm_context_restore;
    term_full_refresh = gterm_full_refresh;

    term_backend = VBE;
}
