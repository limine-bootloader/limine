#ifndef __LIB__BLIB_H__
#define __LIB__BLIB_H__

#include <stddef.h>
#include <stdint.h>

void pit_sleep(uint64_t pit_ticks);

void print(const char *fmt, ...);
char getchar(void);
void gets(char *buf, size_t limit);
uint64_t strtoui(const char *s);

#define DIV_ROUNDUP(a, b) (((a) + ((b) - 1)) / (b))

typedef void *symbol[];

#endif
