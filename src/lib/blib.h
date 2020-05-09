#ifndef __LIB__BLIB_H__
#define __LIB__BLIB_H__

#include <stddef.h>
#include <stdint.h>

void is_valid_memory_range(size_t base, size_t size);

uint8_t bcd_to_int(uint8_t val);

int cpuid(uint32_t leaf, uint32_t subleaf,
          uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

__attribute__((noreturn)) void panic(const char *str);

void pit_sleep(uint64_t pit_ticks);
int pit_sleep_and_quit_on_keypress(uint32_t pit_ticks);

void brewind(size_t count);
void *balloc(size_t count);
void *balloc_aligned(size_t count, size_t alignment);

#define GETCHAR_CURSOR_LEFT  (-10)
#define GETCHAR_CURSOR_RIGHT (-11)
#define GETCHAR_CURSOR_UP    (-12)
#define GETCHAR_CURSOR_DOWN  (-13)

int getchar(void);
void gets(const char *orig_str, char *buf, size_t limit);
uint64_t strtoui(const char *s);

#define DIV_ROUNDUP(a, b) (((a) + ((b) - 1)) / (b))

typedef void *symbol[];

#define SIZEOF_ARRAY(array) (sizeof(array) / sizeof(array[0]))

#endif
