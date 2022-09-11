#ifndef __CRYPT__BLAKE2B_H__
#define __CRYPT__BLAKE2B_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define BLAKE2B_OUT_BYTES 64

void blake2b(void *out, const void *in, size_t in_len);

#endif
