#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <lib/libc.h>
#include <lib/misc.h>

// Slightly adapted strtoul() implementation from FreeBSD.
// https://github.com/freebsd/freebsd-src/blob/de1aa3dab23c06fec962a14da3e7b4755c5880cf/lib/libc/stdlib/strtoul.c
unsigned long strtoul(const char *nptr, char **endptr, int base) {
    const char *s;
    unsigned long acc;
    char c;
    unsigned long cutoff;
    int neg, any, cutlim;

    s = nptr;
    do {
        c = *s++;
    } while (isspace((unsigned char)c));
    if (c == '-') {
        neg = 1;
        c = *s++;
    } else {
        neg = 0;
        if (c == '+')
            c = *s++;
    }
    if ((base == 0 || base == 16) &&
        c == '0' && (*s == 'x' || *s == 'X') &&
        ((s[1] >= '0' && s[1] <= '9') ||
        (s[1] >= 'A' && s[1] <= 'F') ||
        (s[1] >= 'a' && s[1] <= 'f'))) {
        c = s[1];
        s += 2;
        base = 16;
    }
    if (base == 0)
        base = c == '0' ? 8 : 10;
    acc = any = 0;
    if (base < 2 || base > 36)
        goto noconv;

    cutoff = ULONG_MAX / base;
    cutlim = ULONG_MAX % base;
    for ( ; ; c = *s++) {
        if (c >= '0' && c <= '9')
            c -= '0';
        else if (c >= 'A' && c <= 'Z')
            c -= 'A' - 10;
        else if (c >= 'a' && c <= 'z')
            c -= 'a' - 10;
        else
            break;
        if (c >= base)
            break;
        if (any < 0 || acc > cutoff || (acc == cutoff && c > cutlim))
            any = -1;
        else {
            any = 1;
            acc *= base;
            acc += c;
        }
    }
    if (any < 0) {
        acc = ULONG_MAX;
        //errno = ERANGE;
    } else if (!any) {
noconv:
        ;//errno = EINVAL;
    } else if (neg)
        acc = -acc;
    if (endptr != NULL)
        *endptr = (char *)(any ? s - 1 : nptr);
    return (acc);
}

size_t strnlen(const char *str, size_t maxlen) {
    size_t len;

    for (len = 0; len < maxlen && str[len]; len++);

    return len;
}

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
