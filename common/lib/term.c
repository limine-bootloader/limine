#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/image.h>
#include <lib/misc.h>
#include <lib/gterm.h>
#include <drivers/vga_textmode.h>
#include <lib/print.h>
#include <mm/pmm.h>

extern void reset_term(void);
extern void set_cursor_pos_helper(size_t x, size_t y);

void term_deinit(void) {
    switch (term_backend) {
        case VBE:
            gterm_deinit();
    }

    term_notready();
}

void term_vbe(char *config, size_t width, size_t height) {
    if (term_backend != VBE) {
        term_deinit();
    }

    if (quiet || allocations_disallowed) {
        return;
    }

    if (!gterm_init(config, &term_rows, &term_cols, width, height)) {
#if bios == 1
        // Failed to set VBE properly, default to text mode
        term_textmode();
#endif
        return;
    }

    if (serial) {
        term_cols = term_cols > 80 ? 80 : term_cols;
        term_rows = term_rows > 24 ? 24 : term_rows;
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
    term_revscroll = gterm_revscroll;
    term_swap_palette = gterm_swap_palette;
    term_save_state = gterm_save_state;
    term_restore_state = gterm_restore_state;

    term_double_buffer_flush = gterm_double_buffer_flush;

    term_context_size = gterm_context_size;
    term_context_save = gterm_context_save;
    term_context_restore = gterm_context_restore;
    term_full_refresh = gterm_full_refresh;

    term_backend = VBE;
}

// Tries to implement this standard for terminfo
// https://man7.org/linux/man-pages/man4/console_codes.4.html

uint64_t term_arg = 0;
void (*term_callback)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t) = NULL;

struct term_context term_context;

#define escape_offset term_context.escape_offset
#define control_sequence term_context.control_sequence
#define csi term_context.csi
#define escape term_context.escape
#define rrr term_context.rrr
#define discard_next term_context.discard_next
#define bold term_context.bold
#define reverse_video term_context.reverse_video
#define dec_private term_context.dec_private
#define esc_values term_context.esc_values
#define esc_values_i term_context.esc_values_i
#define saved_cursor_x term_context.saved_cursor_x
#define saved_cursor_y term_context.saved_cursor_y
#define current_primary term_context.current_primary
#define insert_mode term_context.insert_mode
#define scroll_top_margin term_context.scroll_top_margin
#define scroll_bottom_margin term_context.scroll_bottom_margin
#define current_charset term_context.current_charset
#define charsets term_context.charsets
#define g_select term_context.g_select

#define saved_state_bold term_context.saved_state_bold
#define saved_state_reverse_video term_context.saved_state_reverse_video
#define saved_state_current_charset term_context.saved_state_current_charset
#define saved_state_current_primary term_context.saved_state_current_primary

#define CHARSET_DEFAULT 0
#define CHARSET_DEC_SPECIAL 1

void term_reinit(void) {
    escape_offset = 0;
    control_sequence = false;
    csi = false;
    escape = false;
    rrr = false;
    discard_next = false;
    bold = false;
    reverse_video = false;
    dec_private = false;
    esc_values_i = 0;
    saved_cursor_x = 0;
    saved_cursor_y = 0;
    current_primary = (size_t)-1;
    insert_mode = false;
    scroll_top_margin = 0;
    scroll_bottom_margin = term_rows;
    current_charset = 0;
    g_select = 0;
    charsets[0] = CHARSET_DEFAULT;
    charsets[1] = CHARSET_DEC_SPECIAL;
    term_autoflush = true;
}

#if bios == 1
void term_textmode(void) {
    term_deinit();

    if (quiet || allocations_disallowed) {
        return;
    }

    init_vga_textmode(&term_rows, &term_cols, true);

    if (serial) {
        term_cols = term_cols > 80 ? 80 : term_cols;
        term_rows = term_rows > 24 ? 24 : term_rows;
    }

    term_reinit();

    raw_putchar    = text_putchar;
    clear          = text_clear;
    enable_cursor  = text_enable_cursor;
    disable_cursor = text_disable_cursor;
    set_cursor_pos = text_set_cursor_pos;
    get_cursor_pos = text_get_cursor_pos;
    set_text_fg    = text_set_text_fg;
    set_text_bg    = text_set_text_bg;
    set_text_fg_bright = text_set_text_fg_bright;
    set_text_bg_bright = text_set_text_bg_bright;
    set_text_fg_default = text_set_text_fg_default;
    set_text_bg_default = text_set_text_bg_default;
    scroll_disable = text_scroll_disable;
    scroll_enable  = text_scroll_enable;
    term_move_character = text_move_character;
    term_scroll = text_scroll;
    term_revscroll = text_revscroll;
    term_swap_palette = text_swap_palette;
    term_save_state = text_save_state;
    term_restore_state = text_restore_state;

    term_double_buffer_flush = text_double_buffer_flush;

    term_context_size = text_context_size;
    term_context_save = text_context_save;
    term_context_restore = text_context_restore;
    term_full_refresh = text_full_refresh;

    term_backend = TEXTMODE;
}
#endif

static uint64_t context_size(void) {
    uint64_t ret = 0;

    ret += sizeof(struct term_context);
    ret += term_context_size();

    return ret;
}

static void context_save(uint64_t ptr) {
    memcpy32to64(ptr, (uint64_t)(uintptr_t)&term_context, sizeof(struct term_context));
    ptr += sizeof(struct term_context);

    term_context_save(ptr);
}

static void context_restore(uint64_t ptr) {
    memcpy32to64((uint64_t)(uintptr_t)&term_context, ptr, sizeof(struct term_context));
    ptr += sizeof(struct term_context);

    term_context_restore(ptr);
}

#if defined (__i386__)
#define TERM_XFER_CHUNK 8192

static uint8_t xfer_buf[TERM_XFER_CHUNK];
#endif

bool term_autoflush = true;

void term_write(uint64_t buf, uint64_t count) {
    if (term_backend == NOT_READY)
        return;

    switch (count) {
        case TERM_CTX_SIZE: {
            uint64_t ret = context_size();
            memcpy32to64(buf, (uint64_t)(uintptr_t)&ret, sizeof(uint64_t));
            return;
        }
        case TERM_CTX_SAVE: {
            context_save(buf);
            return;
        }
        case TERM_CTX_RESTORE: {
            context_restore(buf);
            return;
        }
        case TERM_FULL_REFRESH: {
            term_full_refresh();
            return;
        }
    }

    bool native = false;
#if defined (__x86_64__) || defined (__aarch64__)
    native = true;
#elif !defined (__i386__)
#error Unknown architecture
#endif

    if (!term_runtime || native) {
        const char *s = (const char *)(uintptr_t)buf;

        for (size_t i = 0; i < count; i++)
            term_putchar(s[i]);
    } else {
#if defined (__i386__)
        while (count != 0) {
            uint64_t chunk;
            if (count > TERM_XFER_CHUNK) {
                chunk = TERM_XFER_CHUNK;
            } else {
                chunk = count;
            }

            memcpy32to64((uint64_t)(uintptr_t)xfer_buf, buf, chunk);

            for (size_t i = 0; i < chunk; i++)
                term_putchar(xfer_buf[i]);

            count -= chunk;
            buf += chunk;
        }
#endif
    }

    if (term_autoflush) {
        term_double_buffer_flush();
    }
}

static void sgr(void) {
    size_t i = 0;

    if (!esc_values_i)
        goto def;

    for (; i < esc_values_i; i++) {
        size_t offset;

        if (esc_values[i] == 0) {
def:
            if (reverse_video) {
                reverse_video = false;
                term_swap_palette();
            }
            bold = false;
            current_primary = (size_t)-1;
            set_text_bg_default();
            set_text_fg_default();
            continue;
        }

        else if (esc_values[i] == 1) {
            bold = true;
            if (current_primary != (size_t)-1) {
                if (!reverse_video) {
                    set_text_fg_bright(current_primary);
                } else {
                    set_text_bg_bright(current_primary);
                }
            }
            continue;
        }

        else if (esc_values[i] == 22) {
            bold = false;
            if (current_primary != (size_t)-1) {
                if (!reverse_video) {
                    set_text_fg(current_primary);
                } else {
                    set_text_bg(current_primary);
                }
            }
            continue;
        }

        else if (esc_values[i] >= 30 && esc_values[i] <= 37) {
            offset = 30;
            current_primary = esc_values[i] - offset;

            if (reverse_video) {
                goto set_bg;
            }

set_fg:
            if (bold && !reverse_video) {
                set_text_fg_bright(esc_values[i] - offset);
            } else {
                set_text_fg(esc_values[i] - offset);
            }
            continue;
        }

        else if (esc_values[i] >= 40 && esc_values[i] <= 47) {
            offset = 40;
            if (reverse_video) {
                goto set_fg;
            }

set_bg:
            if (bold && reverse_video) {
                set_text_bg_bright(esc_values[i] - offset);
            } else {
                set_text_bg(esc_values[i] - offset);
            }
            continue;
        }

        else if (esc_values[i] >= 90 && esc_values[i] <= 97) {
            offset = 90;
            current_primary = esc_values[i] - offset;

            if (reverse_video) {
                goto set_bg_bright;
            }

set_fg_bright:
            set_text_fg_bright(esc_values[i] - offset);
            continue;
        }

        else if (esc_values[i] >= 100 && esc_values[i] <= 107) {
            offset = 100;
            if (reverse_video) {
                goto set_fg_bright;
            }

set_bg_bright:
            set_text_bg_bright(esc_values[i] - offset);
            continue;
        }

        else if (esc_values[i] == 39) {
            current_primary = (size_t)-1;

            if (reverse_video) {
                term_swap_palette();
            }

            set_text_fg_default();

            if (reverse_video) {
                term_swap_palette();
            }

            continue;
        }

        else if (esc_values[i] == 49) {
            if (reverse_video) {
                term_swap_palette();
            }

            set_text_bg_default();

            if (reverse_video) {
                term_swap_palette();
            }

            continue;
        }

        else if (esc_values[i] == 7) {
            if (!reverse_video) {
                reverse_video = true;
                term_swap_palette();
            }
            continue;
        }

        else if (esc_values[i] == 27) {
            if (reverse_video) {
                reverse_video = false;
                term_swap_palette();
            }
            continue;
        }
    }
}

static void dec_private_parse(uint8_t c) {
    dec_private = false;

    if (esc_values_i == 0) {
        return;
    }

    bool set;

    switch (c) {
        case 'h':
            set = true; break;
        case 'l':
            set = false; break;
        default:
            return;
    }

    switch (esc_values[0]) {
        case 25: {
            if (set) {
                enable_cursor();
            } else {
                disable_cursor();
            }
            return;
        }
    }

    if (term_callback != NULL) {
        if (term_arg != 0) {
            term_callback(term_arg, TERM_CB_DEC, esc_values_i, (uintptr_t)esc_values, c);
        } else {
            term_callback(TERM_CB_DEC, esc_values_i, (uintptr_t)esc_values, c, 0);
        }
    }
}

static void linux_private_parse(void) {
    if (esc_values_i == 0) {
        return;
    }

    if (term_callback != NULL) {
        if (term_arg != 0) {
            term_callback(term_arg, TERM_CB_LINUX, esc_values_i, (uintptr_t)esc_values, 0);
        } else {
            term_callback(TERM_CB_LINUX, esc_values_i, (uintptr_t)esc_values, 0, 0);
        }
    }
}

static void mode_toggle(uint8_t c) {
    if (esc_values_i == 0) {
        return;
    }

    bool set;

    switch (c) {
        case 'h':
            set = true; break;
        case 'l':
            set = false; break;
        default:
            return;
    }

    switch (esc_values[0]) {
        case 4:
            insert_mode = set; return;
    }

    if (term_callback != NULL) {
        if (term_arg != 0) {
            term_callback(term_arg, TERM_CB_MODE, esc_values_i, (uintptr_t)esc_values, c);
        } else {
            term_callback(TERM_CB_MODE, esc_values_i, (uintptr_t)esc_values, c, 0);
        }
    }
}

static void control_sequence_parse(uint8_t c) {
    if (escape_offset == 2) {
        switch (c) {
            case '[':
                discard_next = true;
                goto cleanup;
            case '?':
                dec_private = true;
                return;
        }
    }

    if (c >= '0' && c <= '9') {
        if (esc_values_i == MAX_ESC_VALUES) {
            return;
        }
        rrr = true;
        esc_values[esc_values_i] *= 10;
        esc_values[esc_values_i] += c - '0';
        return;
    }

    if (rrr == true) {
        esc_values_i++;
        rrr = false;
        if (c == ';')
            return;
    } else if (c == ';') {
        if (esc_values_i == MAX_ESC_VALUES) {
            return;
        }
        esc_values[esc_values_i] = 0;
        esc_values_i++;
        return;
    }

    size_t esc_default;
    switch (c) {
        case 'J': case 'K': case 'q':
            esc_default = 0; break;
        default:
            esc_default = 1; break;
    }

    for (size_t i = esc_values_i; i < MAX_ESC_VALUES; i++) {
        esc_values[i] = esc_default;
    }

    if (dec_private == true) {
        dec_private_parse(c);
        goto cleanup;
    }

    bool r = scroll_disable();
    size_t x, y;
    get_cursor_pos(&x, &y);

    switch (c) {
        case 'F':
            x = 0;
            // FALLTHRU
        case 'A': {
            if (esc_values[0] > y)
                esc_values[0] = y;
            size_t orig_y = y;
            size_t dest_y = y - esc_values[0];
            bool will_be_in_scroll_region = false;
            if ((scroll_top_margin >= dest_y && scroll_top_margin <= orig_y)
             || (scroll_bottom_margin >= dest_y && scroll_bottom_margin <= orig_y)) {
                will_be_in_scroll_region = true;
            }
            if (will_be_in_scroll_region && dest_y < scroll_top_margin) {
                dest_y = scroll_top_margin;
            }
            set_cursor_pos(x, dest_y);
            break;
        }
        case 'E':
            x = 0;
            // FALLTHRU
        case 'e':
        case 'B': {
            if (y + esc_values[0] > term_rows - 1)
                esc_values[0] = (term_rows - 1) - y;
            size_t orig_y = y;
            size_t dest_y = y + esc_values[0];
            bool will_be_in_scroll_region = false;
            if ((scroll_top_margin >= orig_y && scroll_top_margin <= dest_y)
             || (scroll_bottom_margin >= orig_y && scroll_bottom_margin <= dest_y)) {
                will_be_in_scroll_region = true;
            }
            if (will_be_in_scroll_region && dest_y >= scroll_bottom_margin) {
                dest_y = scroll_bottom_margin - 1;
            }
            set_cursor_pos(x, dest_y);
            break;
        }
        case 'a':
        case 'C':
            if (x + esc_values[0] > term_cols - 1)
                esc_values[0] = (term_cols - 1) - x;
            set_cursor_pos(x + esc_values[0], y);
            break;
        case 'D':
            if (esc_values[0] > x)
                esc_values[0] = x;
            set_cursor_pos(x - esc_values[0], y);
            break;
        case 'c':
            if (term_callback != NULL) {
                if (term_arg != 0) {
                    term_callback(term_arg, TERM_CB_PRIVATE_ID, 0, 0, 0);
                } else {
                    term_callback(TERM_CB_PRIVATE_ID, 0, 0, 0, 0);
                }
            }
            break;
        case 'd':
            esc_values[0] -= 1;
            if (esc_values[0] >= term_rows)
                esc_values[0] = term_rows - 1;
            set_cursor_pos(x, esc_values[0]);
            break;
        case 'G':
        case '`':
            esc_values[0] -= 1;
            if (esc_values[0] >= term_cols)
                esc_values[0] = term_cols - 1;
            set_cursor_pos(esc_values[0], y);
            break;
        case 'H':
        case 'f':
            esc_values[0] -= 1;
            esc_values[1] -= 1;
            if (esc_values[1] >= term_cols)
                esc_values[1] = term_cols - 1;
            if (esc_values[0] >= term_rows)
                esc_values[0] = term_rows - 1;
            set_cursor_pos(esc_values[1], esc_values[0]);
            break;
        case 'n':
            switch (esc_values[0]) {
                case 5:
                    if (term_callback != NULL) {
                        if (term_arg != 0) {
                            term_callback(term_arg, TERM_CB_STATUS_REPORT, 0, 0, 0);
                        } else {
                            term_callback(TERM_CB_STATUS_REPORT, 0, 0, 0, 0);
                        }
                    }
                    break;
                case 6:
                    if (term_callback != NULL) {
                        if (term_arg != 0) {
                            term_callback(term_arg, TERM_CB_POS_REPORT, x + 1, y + 1, 0);
                        } else {
                            term_callback(TERM_CB_POS_REPORT, x + 1, y + 1, 0, 0);
                        }
                    }
                    break;
            }
            break;
        case 'q':
            if (term_callback != NULL) {
                if (term_arg != 0) {
                    term_callback(term_arg, TERM_CB_KBD_LEDS, esc_values[0], 0, 0);
                } else {
                    term_callback(TERM_CB_KBD_LEDS, esc_values[0], 0, 0, 0);
                }
            }
            break;
        case 'J':
            switch (esc_values[0]) {
                case 0: {
                    size_t rows_remaining = term_rows - (y + 1);
                    size_t cols_diff = term_cols - (x + 1);
                    size_t to_clear = rows_remaining * term_cols + cols_diff;
                    for (size_t i = 0; i < to_clear; i++) {
                        raw_putchar(' ');
                    }
                    set_cursor_pos(x, y);
                    break;
                }
                case 1: {
                    set_cursor_pos(0, 0);
                    bool b = false;
                    for (size_t yc = 0; yc < term_rows; yc++) {
                        for (size_t xc = 0; xc < term_cols; xc++) {
                            raw_putchar(' ');
                            if (xc == x && yc == y) {
                                set_cursor_pos(x, y);
                                b = true;
                                break;
                            }
                        }
                        if (b == true)
                            break;
                    }
                    break;
                }
                case 2:
                case 3:
                    clear(false);
                    break;
            }
            break;
        case '@':
            for (size_t i = term_cols - 1; ; i--) {
                term_move_character(i + esc_values[0], y, i, y);
                set_cursor_pos(i, y);
                raw_putchar(' ');
                if (i == x) {
                    break;
                }
            }
            set_cursor_pos(x, y);
            break;
        case 'P':
            for (size_t i = x + esc_values[0]; i < term_cols; i++)
                term_move_character(i - esc_values[0], y, i, y);
            set_cursor_pos(term_cols - esc_values[0], y);
            // FALLTHRU
        case 'X':
            for (size_t i = 0; i < esc_values[0]; i++)
                raw_putchar(' ');
            set_cursor_pos(x, y);
            break;
        case 'm':
            sgr();
            break;
        case 's':
            get_cursor_pos(&saved_cursor_x, &saved_cursor_y);
            break;
        case 'u':
            set_cursor_pos(saved_cursor_x, saved_cursor_y);
            break;
        case 'K':
            switch (esc_values[0]) {
                case 0: {
                    for (size_t i = x; i < term_cols; i++)
                        raw_putchar(' ');
                    set_cursor_pos(x, y);
                    break;
                }
                case 1: {
                    set_cursor_pos(0, y);
                    for (size_t i = 0; i < x; i++)
                        raw_putchar(' ');
                    break;
                }
                case 2: {
                    set_cursor_pos(0, y);
                    for (size_t i = 0; i < term_cols; i++)
                        raw_putchar(' ');
                    set_cursor_pos(x, y);
                    break;
                }
            }
            break;
        case 'r':
            scroll_top_margin = 0;
            scroll_bottom_margin = term_rows;
            if (esc_values_i > 0) {
                scroll_top_margin = esc_values[0] - 1;
            }
            if (esc_values_i > 1) {
                scroll_bottom_margin = esc_values[1];
            }
            if (scroll_top_margin >= term_rows
             || scroll_bottom_margin > term_rows
             || scroll_top_margin >= (scroll_bottom_margin - 1)) {
                scroll_top_margin = 0;
                scroll_bottom_margin = term_rows;
            }
            set_cursor_pos(0, 0);
            break;
        case 'l':
        case 'h':
            mode_toggle(c);
            break;
        case ']':
            linux_private_parse();
            break;
    }

    if (r)
        scroll_enable();

cleanup:
    control_sequence = false;
    escape = false;
}

static void restore_state(void) {
    bold = saved_state_bold;
    reverse_video = saved_state_reverse_video;
    current_charset = saved_state_current_charset;
    current_primary = saved_state_current_primary;

    term_restore_state();
}

static void save_state(void) {
    term_save_state();

    saved_state_bold = bold;
    saved_state_reverse_video = reverse_video;
    saved_state_current_charset = current_charset;
    saved_state_current_primary = current_primary;
}

static void escape_parse(uint8_t c) {
    escape_offset++;

    if (control_sequence == true) {
        control_sequence_parse(c);
        return;
    }

    if (csi == true) {
        csi = false;
        goto is_csi;
    }

    size_t x, y;
    get_cursor_pos(&x, &y);

    switch (c) {
        case '[':
is_csi:
            for (size_t i = 0; i < MAX_ESC_VALUES; i++)
                esc_values[i] = 0;
            esc_values_i = 0;
            rrr = false;
            control_sequence = true;
            return;
        case '7':
            save_state();
            break;
        case '8':
            restore_state();
            break;
        case 'c':
            term_reinit();
            clear(true);
            break;
        case 'D':
            if (y == scroll_bottom_margin - 1) {
                term_scroll();
                set_cursor_pos(x, y);
            } else {
                set_cursor_pos(x, y + 1);
            }
            break;
        case 'E':
            if (y == scroll_bottom_margin - 1) {
                term_scroll();
                set_cursor_pos(0, y);
            } else {
                set_cursor_pos(0, y + 1);
            }
            break;
        case 'M':
            // "Reverse linefeed"
            if (y == scroll_top_margin) {
                term_revscroll();
                set_cursor_pos(0, y);
            } else {
                set_cursor_pos(0, y - 1);
            }
            break;
        case 'Z':
            if (term_callback != NULL) {
                if (term_arg != 0) {
                    term_callback(term_arg, TERM_CB_PRIVATE_ID, 0, 0, 0);
                } else {
                    term_callback(TERM_CB_PRIVATE_ID, 0, 0, 0, 0);
                }
            }
            break;
        case '(':
        case ')':
            g_select = c - '\'';
            break;
        case '\e':
            if (term_runtime == false) {
                raw_putchar(c);
            }
            break;
    }

    escape = false;
}

static uint8_t dec_special_to_cp437(uint8_t c) {
    switch (c) {
        case '`': return 0x04;
        case '0': return 0xdb;
        case '-': return 0x18;
        case ',': return 0x1b;
        case '.': return 0x19;
        case 'a': return 0xb1;
        case 'f': return 0xf8;
        case 'g': return 0xf1;
        case 'h': return 0xb0;
        case 'j': return 0xd9;
        case 'k': return 0xbf;
        case 'l': return 0xda;
        case 'm': return 0xc0;
        case 'n': return 0xc5;
        case 'q': return 0xc4;
        case 's': return 0x5f;
        case 't': return 0xc3;
        case 'u': return 0xb4;
        case 'v': return 0xc1;
        case 'w': return 0xc2;
        case 'x': return 0xb3;
        case 'y': return 0xf3;
        case 'z': return 0xf2;
        case '~': return 0xfa;
        case '_': return 0xff;
        case '+': return 0x1a;
        case '{': return 0xe3;
        case '}': return 0x9c;
    }

    return c;
}

void term_putchar(uint8_t c) {
    if (discard_next || (term_runtime == true && (c == 0x18 || c == 0x1a))) {
        discard_next = false;
        escape = false;
        csi = false;
        control_sequence = false;
        g_select = 0;
        return;
    }

    if (escape == true) {
        escape_parse(c);
        return;
    }

    if (g_select) {
        g_select--;
        switch (c) {
            case 'B':
                charsets[g_select] = CHARSET_DEFAULT; break;
            case '0':
                charsets[g_select] = CHARSET_DEC_SPECIAL; break;
        }
        g_select = 0;
        return;
    }

    size_t x, y;
    get_cursor_pos(&x, &y);

    switch (c) {
        case 0x00:
        case 0x7f:
            return;
        case 0x9b:
            csi = true;
            // FALLTHRU
        case '\e':
            escape_offset = 0;
            escape = true;
            return;
        case '\t':
            if ((x / TERM_TABSIZE + 1) >= term_cols) {
                set_cursor_pos(term_cols - 1, y);
                return;
            }
            set_cursor_pos((x / TERM_TABSIZE + 1) * TERM_TABSIZE, y);
            return;
        case 0x0b:
        case 0x0c:
        case '\n':
            if (y == scroll_bottom_margin - 1) {
                term_scroll();
                set_cursor_pos(0, y);
            } else {
                set_cursor_pos(0, y + 1);
            }
            return;
        case '\b':
            set_cursor_pos(x - 1, y);
            return;
        case '\r':
            set_cursor_pos(0, y);
            return;
        case '\a':
            // The bell is handled by the kernel
            if (term_callback != NULL) {
                if (term_arg != 0) {
                    term_callback(term_arg, TERM_CB_BELL, 0, 0, 0);
                } else {
                    term_callback(TERM_CB_BELL, 0, 0, 0, 0);
                }
            }
            return;
        case 14:
            // Move to G1 set
            current_charset = 1;
            return;
        case 15:
            // Move to G0 set
            current_charset = 0;
            return;
    }

    if (insert_mode == true) {
        for (size_t i = term_cols - 1; ; i--) {
            term_move_character(i + 1, y, i, y);
            if (i == x) {
                break;
            }
        }
    }

    // Translate character set
    switch (charsets[current_charset]) {
        case CHARSET_DEFAULT:
            break;
        case CHARSET_DEC_SPECIAL:
            c = dec_special_to_cp437(c);
    }

    raw_putchar(c);
}
