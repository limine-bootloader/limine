#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/image.h>
#include <lib/blib.h>
#include <drivers/vga_textmode.h>

// Tries to implement this standard for terminfo
// https://man7.org/linux/man-pages/man4/console_codes.4.html

#define TERM_TABSIZE (8)
#define MAX_ESC_VALUES (256)

int current_video_mode = -1;

int term_backend = NOT_READY;

void (*raw_putchar)(uint8_t c);
void (*clear)(bool move);
void (*enable_cursor)(void);
bool (*disable_cursor)(void);
void (*set_cursor_pos)(int x, int y);
void (*get_cursor_pos)(int *x, int *y);
void (*set_text_fg)(int fg);
void (*set_text_bg)(int bg);
bool (*scroll_disable)(void);
void (*scroll_enable)(void);

void (*term_double_buffer)(bool status);
void (*term_double_buffer_flush)(void);

int term_rows, term_cols;

#if bios == 1
void term_textmode(void) {
    term_backend = NOT_READY;

    init_vga_textmode(&term_rows, &term_cols, true);

    raw_putchar    = text_putchar;
    clear          = text_clear;
    enable_cursor  = text_enable_cursor;
    disable_cursor = text_disable_cursor;
    set_cursor_pos = text_set_cursor_pos;
    get_cursor_pos = text_get_cursor_pos;
    set_text_fg    = text_set_text_fg;
    set_text_bg    = text_set_text_bg;
    scroll_disable = text_scroll_disable;
    scroll_enable  = text_scroll_enable;

    term_double_buffer       = text_double_buffer;
    term_double_buffer_flush = text_double_buffer_flush;

    term_backend = TEXTMODE;
}
#endif

void term_deinit(void) {
    term_backend = NOT_READY;
}

static void term_putchar(uint8_t c);

void term_write(const char *buf, size_t count) {
    if (term_backend == NOT_READY)
        return;
    bool old_cur_stat = disable_cursor();
    for (size_t i = 0; i < count; i++)
        term_putchar(buf[i]);
    if (old_cur_stat)
        enable_cursor();
}

static int get_cursor_pos_x(void) {
    int x, y;
    get_cursor_pos(&x, &y);
    return x;
}

static int get_cursor_pos_y(void) {
    int x, y;
    get_cursor_pos(&x, &y);
    return y;
}

static bool control_sequence = false;
static bool escape = false;
static bool rrr = false;
static int esc_values[MAX_ESC_VALUES];
static int esc_values_i = 0;
static int saved_cursor_x = 0, saved_cursor_y = 0;

static void sgr(void) {
    int i = 0;

    if (!esc_values_i)
        goto def;

    for (; i < esc_values_i; i++) {
        if (!esc_values[i]) {
def:
            set_text_bg(8);
            set_text_fg(9);
            continue;
        }

        if (esc_values[i] >= 30 && esc_values[i] <= 37) {
            set_text_fg(esc_values[i] - 30);
            continue;
        }

        if (esc_values[i] >= 40 && esc_values[i] <= 47) {
            set_text_bg(esc_values[i] - 40);
            continue;
        }
    }
}

static void control_sequence_parse(uint8_t c) {
    if (c >= '0' && c <= '9') {
        rrr = true;
        esc_values[esc_values_i] *= 10;
        esc_values[esc_values_i] += c - '0';
        return;
    } else {
        if (rrr == true) {
            esc_values_i++;
            rrr = false;
            if (c == ';')
                return;
        } else if (c == ';') {
            esc_values[esc_values_i] = 1;
            esc_values_i++;
            return;
        }
    }

    // default rest to 1
    for (int i = esc_values_i; i < MAX_ESC_VALUES; i++)
        esc_values[i] = 1;

    switch (c) {
        case 'A':
            if (esc_values[0] > get_cursor_pos_y())
                esc_values[0] = get_cursor_pos_y();
            set_cursor_pos(get_cursor_pos_x(), get_cursor_pos_y() - esc_values[0]);
            break;
        case 'B':
            if ((get_cursor_pos_y() + esc_values[0]) > (term_rows - 1))
                esc_values[0] = (term_rows - 1) - get_cursor_pos_y();
            set_cursor_pos(get_cursor_pos_x(), get_cursor_pos_y() + esc_values[0]);
            break;
        case 'C':
            if ((get_cursor_pos_x() + esc_values[0]) > (term_cols - 1))
                esc_values[0] = (term_cols - 1) - get_cursor_pos_x();
            set_cursor_pos(get_cursor_pos_x() + esc_values[0], get_cursor_pos_y());
            break;
        case 'D':
            if (esc_values[0] > get_cursor_pos_x())
                esc_values[0] = get_cursor_pos_x();
            set_cursor_pos(get_cursor_pos_x() - esc_values[0], get_cursor_pos_y());
            break;
        case 'E':
            if (get_cursor_pos_y() + esc_values[0] >= term_rows)
                set_cursor_pos(0, term_rows - 1);
            else
                set_cursor_pos(0, get_cursor_pos_y() + esc_values[0]);
            break;
        case 'F':
            if (get_cursor_pos_y() - esc_values[0] < 0)
                set_cursor_pos(0, 0);
            else
                set_cursor_pos(0, get_cursor_pos_y() - esc_values[0]);
            break;
        case 'd':
            if (esc_values[0] >= term_rows)
                break;
            set_cursor_pos(get_cursor_pos_x(), esc_values[0]);
            break;
        case 'G':
        case '`':
            if (esc_values[0] >= term_cols)
                break;
            set_cursor_pos(esc_values[0], get_cursor_pos_y());
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
        case 'J':
            switch (esc_values[0]) {
                case 2:
                    clear(false);
                    break;
                default:
                    break;
            }
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
                case 2: {
                    int x, y;
                    get_cursor_pos(&x, &y);
                    set_cursor_pos(0, y);
                    bool r = scroll_disable();
                    for (int i = 0; i < term_cols; i++)
                        raw_putchar(' ');
                    if (r)
                        scroll_enable();
                    set_cursor_pos(x, y);
                    break;
                }
            }
        default:
            break;
    }

    control_sequence = false;
    escape = false;
}

static void escape_parse(uint8_t c) {
    if (control_sequence == true) {
        control_sequence_parse(c);
        return;
    }

    switch (c) {
        case '\e':
            escape = false;
            raw_putchar(c);
            break;
        case '[':
            for (int i = 0; i < MAX_ESC_VALUES; i++)
                esc_values[i] = 0;
            esc_values_i = 0;
            rrr = false;
            control_sequence = true;
            break;
        default:
            escape = false;
            break;
    }
}

static void term_putchar(uint8_t c) {
    if (escape == true) {
        escape_parse(c);
        return;
    }

    switch (c) {
        case '\0':
            break;
        case '\e':
            escape = 1;
            return;
        case '\t':
            if ((get_cursor_pos_x() / TERM_TABSIZE + 1) >= term_cols)
                break;
            set_cursor_pos((get_cursor_pos_x() / TERM_TABSIZE + 1) * TERM_TABSIZE, get_cursor_pos_y());
            break;
        default:
            raw_putchar(c);
            break;
    }
}
