#include <lib/tinf.h>

int (*tinf_gzip_uncompress)(void *dest,
                            const void *source, unsigned int sourceLen);
