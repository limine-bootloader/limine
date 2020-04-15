#ifndef __LIB__BLIB_H__
#define __LIB__BLIB_H__

#include <stddef.h>
#include <stdint.h>
#include <lib/mbr.h>

void panic(const char *str);

void pit_sleep(uint64_t pit_ticks);
int pit_sleep_and_quit_on_keypress(uint64_t pit_ticks);

void *balloc(size_t count);
void *balloc_aligned(size_t count, size_t alignment);

#define GETCHAR_CURSOR_LEFT  (-10)
#define GETCHAR_CURSOR_RIGHT (-11)
#define GETCHAR_CURSOR_UP    (-12)
#define GETCHAR_CURSOR_DOWN  (-13)

void print(const char *fmt, ...);
int getchar(void);
void gets(const char *orig_str, char *buf, size_t limit);
uint64_t strtoui(const char *s);

#define DIV_ROUNDUP(a, b) (((a) + ((b) - 1)) / (b))

typedef void *symbol[];

#endif
