#ifndef __COMPRESS__GZIP_H__
#define __COMPRESS__GZIP_H__

#include <stdint.h>

void *gzip_uncompress(const void *source, uint64_t sourceLen, uint64_t *outsize);

#endif
