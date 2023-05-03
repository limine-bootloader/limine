/*
 * originally from tinfgzip - tiny gzip decompressor
 *
 * Copyright (c) 2003-2019 Joergen Ibsen
 * Copyright (c) 2023 mintsuki and contributors to the Limine project
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 *   1. The origin of this software must not be misrepresented; you must
 *      not claim that you wrote the original software. If you use this
 *      software in a product, an acknowledgment in the product
 *      documentation would be appreciated but is not required.
 *
 *   2. Altered source versions must be plainly marked as such, and must
 *      not be misrepresented as being the original software.
 *
 *   3. This notice may not be removed or altered from any source
 *      distribution.
 */

#include <stdint.h>
#include <stb/stb_image.h>

typedef enum {
    FTEXT    = 1,
    FHCRC    = 2,
    FEXTRA   = 4,
    FNAME    = 8,
    FCOMMENT = 16
} gzip_flag;

void *gzip_uncompress(const void *source, uint64_t sourceLen, uint64_t *outsize) {
    const uint8_t *src = (const uint8_t *) source;
    const uint8_t *start;
    int res;
    uint8_t flg;

    /* -- Check header -- */

    /* Check room for at least 10 byte header and 8 byte trailer */
    if (sourceLen < 18) {
        return NULL;
    }

    /* Check id bytes */
    if (src[0] != 0x1F || src[1] != 0x8B) {
        return NULL;
    }

    /* Check method is deflate */
    if (src[2] != 8) {
        return NULL;
    }

    /* Get flag byte */
    flg = src[3];

    /* Check that reserved bits are zero */
    if (flg & 0xE0) {
        return NULL;
    }

    /* -- Find start of compressed data -- */

    /* Skip base header of 10 bytes */
    start = src + 10;

    /* Skip extra data if present */
    if (flg & FEXTRA) {
        uint64_t xlen = *((uint16_t *)start);

        if (xlen > sourceLen - 12) {
            return NULL;
        }

        start += xlen + 2;
    }

    /* Skip file name if present */
    if (flg & FNAME) {
        do {
            if (((uint64_t)(start - src)) >= sourceLen) {
                return NULL;
            }
        } while (*start++);
    }

    /* Skip file comment if present */
    if (flg & FCOMMENT) {
        do {
            if (((uint64_t)(start - src)) >= sourceLen) {
                return NULL;
            }
        } while (*start++);
    }

    if (flg & FHCRC) {
        start += 2;
    }

    /* -- Get decompressed length -- */

    uint32_t dlen = *((uint32_t *)&src[sourceLen - 4]);

    /* -- Decompress data -- */

    if ((src + sourceLen) - start < 8) {
        return NULL;
    }

    void *buf = ext_mem_alloc(dlen);

    // XXX for some reason certain GZ files made by macOS do not properly decompress with stb_image
    //     unless some skew (19 bytes?) is applied to the buffer. I have no idea why this is the
    //     case but I'd rather have them load somewhat than not load at all.
    for (uint64_t skew = 0; skew < 32; skew++) {
        res = stbi_zlib_decode_noheader_buffer(buf, dlen, (const char *)start + skew, (src + sourceLen) - start - 8 - skew);
        if (res != -1) {
            break;
        }
    }

    if (res == -1) {
        pmm_free(buf, dlen);
        return NULL;
    }

    *outsize = (uint64_t)dlen;
    return buf;
}
