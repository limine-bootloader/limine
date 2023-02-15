#ifndef __COMPRESS__TINFGZIP_H__
#define __COMPRESS__TINFGZIP_H__

int tinf_gzip_uncompress(void *dest, unsigned int limit,
                         const void *source, unsigned int sourceLen);

#endif
