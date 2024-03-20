#ifndef CRYPT__BLAKE2B_H__
#define CRYPT__BLAKE2B_H__

#include <stddef.h>

#define BLAKE2B_OUT_BYTES 64

void blake2b(void *out, const void *in, size_t in_len);

#endif
