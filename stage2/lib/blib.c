#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/term.h>
#include <lib/real.h>
#include <sys/cpu.h>
#include <sys/e820.h>
#include <lib/print.h>
#include <lib/config.h>
#include <mm/pmm.h>

uint8_t boot_drive;

// BIOS partitions are specified in the <BIOS drive>:<partition> form.
// The drive may be omitted, the partition cannot.
static bool parse_bios_partition(char *loc, uint8_t *drive, uint8_t *partition) {
    for (size_t i = 0; ; i++) {
        if (loc[i] == 0)
            return false;

        if (loc[i] == ':') {
            loc[i] = 0;
            if (*loc == 0)
                *drive = boot_drive;
            else
                *drive = strtoui(loc);
            loc += i + 1;
            break;
        }
    }

    if (*loc == 0)
        return false;

    *partition = strtoui(loc);

    return true;
}

static bool uri_bios_dispatch(struct file_handle *fd, char *loc, char *path) {
    uint8_t drive, partition;

    if (!parse_bios_partition(loc, &drive, &partition))
        return false;

    if (fopen(fd, drive, partition, path))
        return false;

    return true;
}

bool uri_open(struct file_handle *fd, char *uri) {
    char *resource, *root, *path;
    config_resolve_uri(uri, &resource, &root, &path);

    if (!strcmp(resource, "bios")) {
        return uri_bios_dispatch(fd, root, path);
    } else {
        panic("Resource `%s` not valid.");
    }
}

// This integer sqrt implementation has been adapted from:
// https://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
uint64_t sqrt(uint64_t a_nInput) {
    uint64_t op  = a_nInput;
    uint64_t res = 0;
    uint64_t one = (uint64_t)1 << 62;

    // "one" starts at the highest power of four <= than the argument.
    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    return res;
}

uint8_t bcd_to_int(uint8_t val) {
    return (val & 0x0f) + ((val & 0xf0) >> 4) * 10;
}

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    asm volatile ("cli" ::: "memory");

    va_list args;

    va_start(args, fmt);

    print("\033[31mPANIC\033[37;1m\033[40m: ");
    vprint(fmt, args);

    va_end(args);

    for (;;) {
        asm volatile ("hlt" ::: "memory");
    }
}

static int char_value(char c) {
    if (c >= 'a' && c <= 'z') {
        return (c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'Z') {
        return (c - 'A') + 10;
    }
    if (c >= '0' && c <= '9'){
        return c - '0';
    }

    return 0;
}

uint64_t strtoui(const char *s) {
    uint64_t n = 0;
    while (*s)
        n = n * 10 + char_value(*(s++));
    return n;
}

uint64_t strtoui16(const char *s) {
    uint64_t n = 0;
    while (*s)
        n = n * 16 + char_value(*(s++));
    return n;
}

int getchar_internal(uint32_t eax) {
    switch ((eax >> 8) & 0xff) {
        case 0x4b:
            return GETCHAR_CURSOR_LEFT;
        case 0x4d:
            return GETCHAR_CURSOR_RIGHT;
        case 0x48:
            return GETCHAR_CURSOR_UP;
        case 0x50:
            return GETCHAR_CURSOR_DOWN;
    }
    return (char)(eax & 0xff);
}

int getchar(void) {
    struct rm_regs r = {0};
    rm_int(0x16, &r, &r);
    return getchar_internal(r.eax);
}

static void gets_reprint_string(int x, int y, const char *s, size_t limit) {
    int last_x, last_y;
    get_cursor_pos(&last_x, &last_y);
    set_cursor_pos(x, y);
    for (size_t i = 0; i < limit; i++) {
        term_write(" ", 1);
    }
    set_cursor_pos(x, y);
    term_write(s, strlen(s));
    set_cursor_pos(last_x, last_y);
}

void gets(const char *orig_str, char *buf, size_t limit) {
    size_t orig_str_len = strlen(orig_str);
    memmove(buf, orig_str, orig_str_len);
    buf[orig_str_len] = 0;

    int orig_x, orig_y;
    get_cursor_pos(&orig_x, &orig_y);

    print("%s", buf);

    for (size_t i = orig_str_len; ; ) {
        int c = getchar();
        switch (c) {
            case GETCHAR_CURSOR_LEFT:
                if (i) {
                    i--;
                    term_write("\b", 1);
                }
                continue;
            case GETCHAR_CURSOR_RIGHT:
                if (i < strlen(buf)) {
                    i++;
                    term_write(" ", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
                continue;
            case '\b':
                if (i) {
                    i--;
                    for (size_t j = i; ; j++) {
                        buf[j] = buf[j+1];
                        if (!buf[j])
                            break;
                    }
                    term_write("\b", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
                continue;
            case '\r':
                term_write("\n", 1);
                return;
            default:
                if (strlen(buf) < limit-1) {
                    for (size_t j = strlen(buf); ; j--) {
                        buf[j+1] = buf[j];
                        if (j == i)
                            break;
                    }
                    buf[i++] = c;
                    term_write(" ", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
        }
    }
}
