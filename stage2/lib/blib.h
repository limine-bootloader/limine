#ifndef __LIB__BLIB_H__
#define __LIB__BLIB_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

extern uint8_t boot_drive;

bool parse_resolution(int *width, int *height, int *bpp, const char *buf);

uint64_t sqrt(uint64_t a_nInput);

int digit_to_int(char c);
uint8_t bcd_to_int(uint8_t val);

__attribute__((noreturn)) void panic(const char *fmt, ...);

int pit_sleep_and_quit_on_keypress(uint32_t pit_ticks);

uint64_t strtoui(const char *s, const char **end, int base);

#define DIV_ROUNDUP(a, b) (((a) + ((b) - 1)) / (b))

#define ALIGN_UP(x, a) ({ \
    typeof(x) value = x; \
    typeof(a) align = a; \
    value = DIV_ROUNDUP(value, align) * align; \
    value; \
})

#define ALIGN_DOWN(x, a) ({ \
    typeof(x) value = x; \
    typeof(a) align = a; \
    value = (value / align) * align; \
    value; \
})

#define SIZEOF_ARRAY(array) (sizeof(array) / sizeof(array[0]))

typedef void *symbol[];

#endif
