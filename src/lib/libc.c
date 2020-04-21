#include <stddef.h>
#include <stdint.h>
#include <lib/libc.h>

void *memcpy(void *dest, const void *src, size_t count) {
    size_t i = 0;

    uint8_t *dest2 = dest;
    const uint8_t *src2 = src;

    for (i = 0; i < count; i++) {
        dest2[i] = src2[i];
    }

    return dest;
}

void *memset(void *s, int c, size_t count) {
    uint8_t *p = s, *end = p + count;
    for (; p != end; p++) {
        *p = (uint8_t)c;
    }

    return s;
}

void *memmove(void *dest, const void *src, size_t count) {
    size_t i = 0;

    uint8_t *dest2 = dest;
    const uint8_t *src2 = src;

    if (src > dest) {
        for (i = 0; i < count; i++) {
            dest2[i] = src2[i];
        }
    } else if (src < dest) {
        for (i = count; i > 0; i--) {
            dest2[i - 1] = src2[i - 1];
        }
    }

    return dest;
}

int memcmp(const void *s1, const void *s2, size_t n) {
    const uint8_t *a = s1;
    const uint8_t *b = s2;

    for (size_t i = 0; i < n; i++) {
        if (a[i] < b[i]) {
            return -1;
        } else if (a[i] > b[i]) {
            return 1;
        }
    }

    return 0;
}

char *strchrnul(const char *s, int c) {
    while (*s)
        if ((*s++) == c)
            break;
    return (char *)s;
}

char *strcpy(char *dest, const char *src) {
    size_t i = 0;

    for (i = 0; src[i]; i++)
        dest[i] = src[i];

    dest[i] = 0;

    return dest;
}

char *strncpy(char *dest, const char *src, size_t cnt) {
    size_t i = 0;

    for (i = 0; i < cnt; i++)
        dest[i] = src[i];

    return dest;
}

int strcmp(const char *dst, const char *src) {
    size_t i;

    for (i = 0; dst[i] == src[i]; i++) {
        if ((!dst[i]) && (!src[i])) return 0;
    }

    return 1;
}

int strncmp(const char *dst, const char *src, size_t count) {
    size_t i;

    for (i = 0; i < count; i++)
        if (dst[i] != src[i]) return 1;

    return 0;
}

size_t strlen(const char *str) {
    size_t len;

    for (len = 0; str[len]; len++);

    return len;
}

char *strtok(char *str, const char *delimiter) {
    static char* buffer;

    if (str != NULL) {
        buffer = str;
    }

    if (buffer[0] == '\0') {
        return NULL;
    }

    char* ret = buffer;

    for (char* b = buffer; *b != '\0'; b++) {
        for (const char* d = delimiter; *d != '\0'; d++) {
            if(*b == *d) {
                *b     = '\0';
                buffer = b + 1;

                // Skip the beginning delimiters
                if (b == ret) {
                    ret++;
                    continue;
                }

                return ret;
            }
        }
    }

    return ret;
}
