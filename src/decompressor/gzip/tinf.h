/*
 * tinf - tiny inflate library (inflate, gzip, zlib)
 *
 * Copyright (c) 2003-2019 Joergen Ibsen
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

#ifndef TINF_H_INCLUDED
#define TINF_H_INCLUDED

#define TINF_VER_MAJOR 1        /**< Major version number */
#define TINF_VER_MINOR 2        /**< Minor version number */
#define TINF_VER_PATCH 1        /**< Patch version number */
#define TINF_VER_STRING "1.2.1" /**< Version number as a string */

/**
 * Status codes returned.
 *
 * @see tinf_uncompress, tinf_gzip_uncompress, tinf_zlib_uncompress
 */
typedef enum {
    TINF_OK         = 0,  /**< Success */
    TINF_DATA_ERROR = -3, /**< Input error */
    TINF_BUF_ERROR  = -5  /**< Not enough room for output */
} tinf_error_code;

/**
 * Decompress `sourceLen` bytes of deflate data from `source` to `dest`.
 *
 * The variable `destLen` points to must contain the size of `dest` on entry,
 * and will be set to the size of the decompressed data on success.
 *
 * Reads at most `sourceLen` bytes from `source`.
 * Writes at most `*destLen` bytes to `dest`.
 *
 * @param dest pointer to where to place decompressed data
 * @param destLen pointer to variable containing size of `dest`
 * @param source pointer to compressed data
 * @param sourceLen size of compressed data
 * @return `TINF_OK` on success, error code on error
 */
int tinf_uncompress(void *dest,
                           const void *source, unsigned int sourceLen);

/**
 * Decompress `sourceLen` bytes of gzip data from `source` to `dest`.
 *
 * The variable `destLen` points to must contain the size of `dest` on entry,
 * and will be set to the size of the decompressed data on success.
 *
 * Reads at most `sourceLen` bytes from `source`.
 * Writes at most `*destLen` bytes to `dest`.
 *
 * @param dest pointer to where to place decompressed data
 * @param destLen pointer to variable containing size of `dest`
 * @param source pointer to compressed data
 * @param sourceLen size of compressed data
 * @return `TINF_OK` on success, error code on error
 */
int tinf_gzip_uncompress(void *dest,
                                const void *source, unsigned int sourceLen);
#endif /* TINF_H_INCLUDED */
