#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <lib/trace.h>
#include <lib/real.h>

uint8_t boot_drive;
int     boot_partition = -1;

bool booted_from_pxe = false;
bool booted_from_cd = false;
bool stage3_loaded = false;

bool parse_resolution(int *width, int *height, int *bpp, const char *buf) {
    int res[3] = {0};

    const char *first = buf;
    for (int i = 0; i < 3; i++) {
        const char *last;
        int x = strtoui(first, &last, 10);
        if (first == last)
            break;
        res[i] = x;
        if (*last == 0)
            break;
        first = last + 1;
    }

    if (res[0] == 0 || res[1] == 0)
        return false;

    if (res[2] == 0)
        res[2] = 32;

    *width = res[0], *height = res[1], *bpp = res[2];

    return true;
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

    print("\n");
    print_stacktrace(NULL);

    rm_hcf();
}

int digit_to_int(char c) {
    if (c >= 'a' && c <= 'f') {
        return (c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return (c - 'A') + 10;
    }
    if (c >= '0' && c <= '9'){
        return c - '0';
    }

    return -1;
}

uint64_t strtoui(const char *s, const char **end, int base) {
    uint64_t n = 0;
    for (size_t i = 0; ; i++) {
        int d = digit_to_int(s[i]);
        if (d == -1) {
            if (end != NULL)
                *end = &s[i];
            break;
        }
        n = n * base + d;
    }
    return n;
}
