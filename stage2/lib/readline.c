#include <stdint.h>
#include <stddef.h>
#include <lib/readline.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/term.h>
#include <lib/real.h>

int getchar_internal(uint32_t eax) {
    switch ((eax >> 8) & 0xff) {
        case 0x44:
            return GETCHAR_F10;
        case 0x4b:
            return GETCHAR_CURSOR_LEFT;
        case 0x4d:
            return GETCHAR_CURSOR_RIGHT;
        case 0x48:
            return GETCHAR_CURSOR_UP;
        case 0x50:
            return GETCHAR_CURSOR_DOWN;
        case 0x53:
            return GETCHAR_DELETE;
    }
    return (char)(eax & 0xff);
}

int getchar(void) {
    struct rm_regs r = {0};
    rm_int(0x16, &r, &r);
    return getchar_internal(r.eax);
}

static void reprint_string(int x, int y, const char *s) {
    int orig_x, orig_y;
    disable_cursor();
    get_cursor_pos(&orig_x, &orig_y);
    set_cursor_pos(x, y);
    term_write(s, strlen(s));
    set_cursor_pos(orig_x, orig_y);
    enable_cursor();
}

static void cursor_back(void) {
    int x, y;
    get_cursor_pos(&x, &y);
    if (x) {
        x--;
    } else if (y) {
        y--;
        x = term_cols - 1;
    }
    set_cursor_pos(x, y);
}

static void cursor_fwd(void) {
    int x, y;
    get_cursor_pos(&x, &y);
    if (x < term_cols - 1) {
        x++;
    } else if (y < term_rows - 1) {
        y++;
        x = 0;
    }
    set_cursor_pos(x, y);
}

void readline(const char *orig_str, char *buf, size_t limit) {
    size_t orig_str_len = strlen(orig_str);
    memmove(buf, orig_str, orig_str_len);
    buf[orig_str_len] = 0;

    int orig_x, orig_y;
    get_cursor_pos(&orig_x, &orig_y);

    term_write(orig_str, orig_str_len);

    for (size_t i = orig_str_len; ; ) {
        int c = getchar();
        switch (c) {
            case GETCHAR_CURSOR_LEFT:
                if (i) {
                    i--;
                    cursor_back();
                }
                continue;
            case GETCHAR_CURSOR_RIGHT:
                if (i < strlen(buf)) {
                    i++;
                    cursor_fwd();
                }
                continue;
            case '\b':
                if (i) {
                    i--;
                    cursor_back();
            case GETCHAR_DELETE:;
                    size_t j;
                    for (j = i; ; j++) {
                        buf[j] = buf[j+1];
                        if (!buf[j]) {
                            buf[j] = ' ';
                            break;
                        }
                    }
                    reprint_string(orig_x, orig_y, buf);
                    buf[j] = 0;
                }
                continue;
            case '\r':
                term_write("\n", 1);
                return;
            default:
                if (strlen(buf) < limit - 1) {
                    for (size_t j = strlen(buf); ; j--) {
                        buf[j+1] = buf[j];
                        if (j == i)
                            break;
                    }
                    buf[i] = c;
                    i++;
                    cursor_fwd();
                    reprint_string(orig_x, orig_y, buf);
                }
        }
    }
}
