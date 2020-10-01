#ifndef __LIB__BLIB_H__
#define __LIB__BLIB_H__

#include <stddef.h>
#include <stdint.h>
#include <fs/file.h>

struct kernel_loc {
    int kernel_drive;
    int kernel_part;
    char *kernel_path;
    struct file_handle *fd;
};

struct kernel_loc get_kernel_loc(int boot_drive);

uint64_t sqrt(uint64_t a_nInput);

uint8_t bcd_to_int(uint8_t val);

int cpuid(uint32_t leaf, uint32_t subleaf,
          uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx);

__attribute__((noreturn)) void panic(const char *fmt, ...);

int pit_sleep_and_quit_on_keypress(uint32_t pit_ticks);

#define GETCHAR_CURSOR_LEFT  (-10)
#define GETCHAR_CURSOR_RIGHT (-11)
#define GETCHAR_CURSOR_UP    (-12)
#define GETCHAR_CURSOR_DOWN  (-13)

int getchar(void);
void gets(const char *orig_str, char *buf, size_t limit);
uint64_t strtoui(const char *s);
uint64_t strtoui16(const char *s);

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
