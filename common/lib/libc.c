#include <stddef.h>
#include <stdint.h>
#include <lib/libc.h>

void *memchr(const void *ptr, int ch, size_t n) {
    uint8_t *p = (uint8_t *)ptr;

    for (size_t i = 0; i < n; i++) {
        if (p[i] == ch) {
            return (void *)ptr + i;
        }
    }

    return NULL;
}

char *strchr(const char *str, int ch) {
    for (size_t i = 0; str[i]; i++) {
        if (str[i] == ch) {
            return (char *)str + i;
        }
    }

    return NULL;
}

char *strrchr(const char *str, int ch) {
    char *p = NULL;

    for (size_t i = 0; str[i]; i++) {
        if (str[i] == ch) {
            p = (char *)str + i;
        }
    }

    return p;
}

size_t strnlen(const char *str, size_t maxlen) {
    size_t len;

    for (len = 0; len < maxlen && str[len]; len++);

    return len;
}
