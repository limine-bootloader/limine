#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <lib/utf.h>

#define REPLACEMENT_CHARACTER 0xfffd

bool utf16_to_utf8(char *dest, size_t dest_size, const uint16_t *src, size_t src_len, size_t *dest_len) {
    bool complete = dest_size != 0;
    size_t len = 0;

    for (size_t i = 0; i < src_len && src[i] != 0; i++) {
        uint32_t cp = src[i];

        if (cp >= 0xd800 && cp <= 0xdfff) {
            if (cp <= 0xdbff && i + 1 < src_len && src[i + 1] >= 0xdc00 && src[i + 1] <= 0xdfff) {
                cp = 0x10000 + ((cp - 0xd800) << 10) + (src[i + 1] - 0xdc00);
                i++;
            } else {
                cp = REPLACEMENT_CHARACTER;
            }
        }

        size_t n = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        // Leave room for the NUL.
        if (dest_size - len <= n) {
            complete = false;
            break;
        }

        if (n == 1) {
            dest[len++] = cp;
            continue;
        }

        // A lead byte has its top n bits set, then a clear one.
        size_t shift = 6 * (n - 1);
        dest[len++] = (uint8_t)(0xff00 >> n) | (cp >> shift);
        while (shift != 0) {
            shift -= 6;
            dest[len++] = 0x80 | ((cp >> shift) & 0x3f);
        }
    }

    if (dest_size != 0) {
        dest[len] = 0;
    }
    if (dest_len != NULL) {
        *dest_len = len;
    }
    return complete;
}

bool utf8_to_utf16(uint16_t *dest, size_t dest_size, const char *src, size_t src_len, size_t *dest_len) {
    const uint8_t *s = (const uint8_t *)src;
    bool complete = dest_size != 0;
    size_t len = 0;

    for (size_t i = 0; i < src_len && s[i] != 0;) {
        uint32_t cp = s[i++];
        size_t trail = 0;
        // The narrower bounds of a first trail byte exclude overlong forms,
        // surrogates, and code points above U+10FFFF (Unicode Table 3-7).
        uint8_t lo = 0x80, hi = 0xbf;

        if (cp >= 0xc2 && cp <= 0xdf) {
            trail = 1;
            cp &= 0x1f;
        } else if (cp >= 0xe0 && cp <= 0xef) {
            trail = 2;
            lo = cp == 0xe0 ? 0xa0 : 0x80;
            hi = cp == 0xed ? 0x9f : 0xbf;
            cp &= 0x0f;
        } else if (cp >= 0xf0 && cp <= 0xf4) {
            trail = 3;
            lo = cp == 0xf0 ? 0x90 : 0x80;
            hi = cp == 0xf4 ? 0x8f : 0xbf;
            cp &= 0x07;
        } else if (cp >= 0x80) {
            cp = REPLACEMENT_CHARACTER;
        }

        // A byte that cannot continue the sequence is left for the next one,
        // so each maximal ill-formed subpart becomes a single U+FFFD, as the
        // Unicode Standard recommends.
        for (; trail != 0; trail--) {
            if (i >= src_len || s[i] < lo || s[i] > hi) {
                cp = REPLACEMENT_CHARACTER;
                break;
            }
            cp = (cp << 6) | (s[i++] & 0x3f);
            lo = 0x80;
            hi = 0xbf;
        }

        size_t n = cp >= 0x10000 ? 2 : 1;
        // Leave room for the NUL.
        if (dest_size - len <= n) {
            complete = false;
            break;
        }

        if (n == 2) {
            cp -= 0x10000;
            dest[len++] = 0xd800 | (cp >> 10);
            dest[len++] = 0xdc00 | (cp & 0x3ff);
        } else {
            dest[len++] = cp;
        }
    }

    if (dest_size != 0) {
        dest[len] = 0;
    }
    if (dest_len != NULL) {
        *dest_len = len;
    }
    return complete;
}
