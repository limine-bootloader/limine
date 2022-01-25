#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <lib/term.h>
#include <lib/libc.h>
#if bios == 1
#include <lib/real.h>
#endif
#include <sys/cpu.h>

#if bios == 1
static void s2_print(const char *s, size_t len) {
    for (size_t i = 0; i < len; i++) {
        struct rm_regs r = {0};
        char c = s[i];

        switch (c) {
            case '\n':
                r.eax = 0x0e00 | '\r';
                rm_int(0x10, &r, &r);
                r = (struct rm_regs){0};
                r.eax = 0x0e00 | '\n';
                rm_int(0x10, &r, &r);
                break;
            default:
                r.eax = 0x0e00 | s[i];
                rm_int(0x10, &r, &r);
                break;
        }
    }
}
#endif

static const char *base_digits = "0123456789abcdef";

#define PRINT_BUF_MAX 4096

static void prn_str(char *print_buf, size_t *print_buf_i, const char *string) {
    size_t i;

    for (i = 0; string[i]; i++) {
        if (*print_buf_i == (PRINT_BUF_MAX - 1))
            break;
        print_buf[(*print_buf_i)++] = string[i];
    }

    print_buf[*print_buf_i] = 0;
}

static void prn_nstr(char *print_buf, size_t *print_buf_i, const char *string, size_t len) {
    size_t i;

    for (i = 0; i < len; i++) {
        if (*print_buf_i == (PRINT_BUF_MAX - 1))
            break;
        print_buf[(*print_buf_i)++] = string[i];
    }

    print_buf[*print_buf_i] = 0;
}

static void prn_char(char *print_buf, size_t *print_buf_i, char c) {
    if (*print_buf_i < (PRINT_BUF_MAX - 1)) {
        print_buf[(*print_buf_i)++] = c;
    }

    print_buf[*print_buf_i] = 0;
}

static void prn_i(char *print_buf, size_t *print_buf_i, int64_t x) {
    int i;
    char buf[20] = {0};

    if (!x) {
        prn_char(print_buf, print_buf_i, '0');
        return;
    }

    int sign = x < 0;
    if (sign) x = -x;

    for (i = 18; x; i--) {
        buf[i] = (x % 10) + 0x30;
        x = x / 10;
    }
    if (sign)
        buf[i] = '-';
    else
        i++;

    prn_str(print_buf, print_buf_i, buf + i);
}

static void prn_ui(char *print_buf, size_t *print_buf_i, uint64_t x) {
    int i;
    char buf[21] = {0};

    if (!x) {
        prn_char(print_buf, print_buf_i, '0');
        return;
    }

    for (i = 19; x; i--) {
        buf[i] = (x % 10) + 0x30;
        x = x / 10;
    }

    i++;
    prn_str(print_buf, print_buf_i, buf + i);
}

static void prn_x(char *print_buf, size_t *print_buf_i, uint64_t x) {
    int i;
    char buf[17] = {0};

    if (!x) {
        prn_str(print_buf, print_buf_i, "0x0");
        return;
    }

    for (i = 15; x; i--) {
        buf[i] = base_digits[(x % 16)];
        x = x / 16;
    }

    i++;
    prn_str(print_buf, print_buf_i, "0x");
    prn_str(print_buf, print_buf_i, buf + i);
}

void print(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);
    vprint(fmt, args);
    va_end(args);
}

static char print_buf[PRINT_BUF_MAX];

void vprint(const char *fmt, va_list args) {
    static bool com_initialised = false;

    if (COM_OUTPUT && !com_initialised) {
        // Init com1
        outb(0x3F8 + 1, 0x00);
        outb(0x3F8 + 3, 0x80);
        outb(0x3F8 + 0, 0x0c); // 9600 baud
        outb(0x3F8 + 1, 0x00);
        outb(0x3F8 + 3, 0x03);
        outb(0x3F8 + 2, 0xc7);
        outb(0x3F8 + 4, 0x0b);

        com_initialised = true;
    }

    size_t print_buf_i = 0;

    for (;;) {
        while (*fmt && *fmt != '%')
            prn_char(print_buf, &print_buf_i, *fmt++);

        if (!*fmt++)
            goto out;

        switch (*fmt++) {
            case 's': {
                char *str = (char *)va_arg(args, const char *);
                if (!str)
                    prn_str(print_buf, &print_buf_i, "(null)");
                else
                    prn_str(print_buf, &print_buf_i, str); }
                break;
            case 'S': {
                char *str = (char *)va_arg(args, const char *);
                size_t str_len = va_arg(args, size_t);
                if (!str)
                    prn_str(print_buf, &print_buf_i, "(null)");
                else
                    prn_nstr(print_buf, &print_buf_i, str, str_len); }
                break;
            case 'd':
                prn_i(print_buf, &print_buf_i, (int64_t)va_arg(args, int32_t));
                break;
            case 'u':
                prn_ui(print_buf, &print_buf_i, (uint64_t)va_arg(args, uint32_t));
                break;
            case 'x':
                prn_x(print_buf, &print_buf_i, (uint64_t)va_arg(args, uint32_t));
                break;
            case 'D':
                prn_i(print_buf, &print_buf_i, va_arg(args, int64_t));
                break;
            case 'U':
                prn_ui(print_buf, &print_buf_i, va_arg(args, uint64_t));
                break;
            case 'X':
                prn_x(print_buf, &print_buf_i, va_arg(args, uint64_t));
                break;
            case 'p':
                prn_x(print_buf, &print_buf_i, va_arg(args, uintptr_t));
                break;
            case 'c': {
                char c = (char)va_arg(args, int);
                prn_char(print_buf, &print_buf_i, c); }
                break;
            default:
                prn_char(print_buf, &print_buf_i, '?');
                break;
        }
    }

out:
#if bios == 1
    if (stage3_loaded) {
#endif
        term_write((uint64_t)(uintptr_t)print_buf, print_buf_i);
#if bios == 1
    } else {
        s2_print(print_buf, print_buf_i);
    }
#endif

    for (size_t i = 0; i < print_buf_i; i++) {
        if (E9_OUTPUT) {
            outb(0xe9, print_buf[i]);
        }
        if (COM_OUTPUT) {
            if (print_buf[i] == '\n') {
                while ((inb(0x3f8 + 5) & 0x20) == 0);
                outb(0x3f8, '\r');
            }
            while ((inb(0x3f8 + 5) & 0x20) == 0);
            outb(0x3f8, print_buf[i]);
        }
    }
}
