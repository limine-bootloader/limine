#ifndef __COMPRESS__TINFGZIP_H__
#define __COMPRESS__TINFGZIP_H__

#include <stdint.h>

void *tinf_gzip_uncompress(const void *source, uint64_t sourceLen, uint64_t *outsize);

#endif
