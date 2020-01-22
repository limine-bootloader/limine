#ifndef __LIB__BLIB_H__
#define __LIB__BLIB_H__

#include <stddef.h>

void print(const char *fmt, ...);
char getchar(void);
void gets(char *buf, size_t limit);

#define DIV_ROUNDUP(a, b) (((a) + ((b) - 1)) / (b))

typedef void *symbol[];

#endif
