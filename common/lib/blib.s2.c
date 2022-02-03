#include <stdint.h>
#include <stddef.h>
#include <lib/blib.h>
#include <lib/print.h>

bool verbose = true;
bool quiet = false;

uint8_t bcd_to_int(uint8_t val) {
    return (val & 0x0f) + ((val & 0xf0) >> 4) * 10;
}
uint8_t int_to_bcd(uint8_t val) {
    return (val % 10) | (val / 10) << 4;
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
