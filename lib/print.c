#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <lib/print.h>
#include <drivers/vga_textmode.h>
#include <drivers/e9.h>

static const char *base_digits = "0123456789abcdef";

#define PRINT_BUF_MAX 512

static void prn_str(char *print_buf, size_t *print_buf_i, const char *string) {
    size_t i;

    for (i = 0; string[i]; i++) {
        if (*print_buf_i == (PRINT_BUF_MAX - 1))
            break;
        print_buf[(*print_buf_i)++] = string[i];
    }

    print_buf[*print_buf_i] = 0;

    return;
}

static void prn_nstr(char *print_buf, size_t *print_buf_i, const char *string, size_t len) {
    size_t i;

    for (i = 0; i < len; i++) {
        if (*print_buf_i == (PRINT_BUF_MAX - 1))
            break;
        print_buf[(*print_buf_i)++] = string[i];
    }

    print_buf[*print_buf_i] = 0;

    return;
}

static void prn_char(char *print_buf, size_t *print_buf_i, char c) {
    if (*print_buf_i < (PRINT_BUF_MAX - 1)) {
        print_buf[(*print_buf_i)++] = c;
    }

    print_buf[*print_buf_i] = 0;

    return;
}

static void prn_i(char *print_buf, size_t *print_buf_i, int32_t x) {
    int i;
    char buf[12] = {0};

    if (!x) {
        prn_char(print_buf, print_buf_i, '0');
        return;
    }

    int sign = x < 0;
    if (sign) x = -x;

    for (i = 10; x; i--) {
        buf[i] = (x % 10) + 0x30;
        x = x / 10;
    }
    if (sign)
        buf[i] = '-';
    else
        i++;

    prn_str(print_buf, print_buf_i, buf + i);

    return;
}

static void prn_ui(char *print_buf, size_t *print_buf_i, uint32_t x) {
    int i;
    char buf[11] = {0};

    if (!x) {
        prn_char(print_buf, print_buf_i, '0');
        return;
    }

    for (i = 9; x; i--) {
        buf[i] = (x % 10) + 0x30;
        x = x / 10;
    }

    i++;
    prn_str(print_buf, print_buf_i, buf + i);

    return;
}

static void prn_x(char *print_buf, size_t *print_buf_i, uint32_t x) {
    int i;
    char buf[9] = {0};

    if (!x) {
        prn_str(print_buf, print_buf_i, "0x0");
        return;
    }

    for (i = 7; x; i--) {
        buf[i] = base_digits[(x % 16)];
        x = x / 16;
    }

    i++;
    prn_str(print_buf, print_buf_i, "0x");
    prn_str(print_buf, print_buf_i, buf + i);

    return;
}

void print(const char *fmt, ...) {
    va_list args;

    va_start(args, fmt);

    char print_buf[PRINT_BUF_MAX];
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
                prn_i(print_buf, &print_buf_i, va_arg(args, int32_t));
                break;
            case 'u':
                prn_ui(print_buf, &print_buf_i, va_arg(args, uint32_t));
                break;
            case 'x':
                prn_x(print_buf, &print_buf_i, va_arg(args, uint32_t));
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
    va_end(args);
    text_write(print_buf, print_buf_i);

    return;
}
