#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/term.h>
#include <lib/real.h>
#include <sys/cpu.h>
#include <sys/e820.h>
#include <lib/print.h>
#include <lib/config.h>
#include <mm/pmm.h>

struct kernel_loc get_kernel_loc(int boot_drive) {
    int kernel_drive; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "KERNEL_DRIVE")) {
            kernel_drive = boot_drive;
        } else {
            kernel_drive = (int)strtoui(buf);
        }
    }

    int kernel_part; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "KERNEL_PARTITION")) {
            panic("KERNEL_PARTITION not specified");
        } else {
            kernel_part = (int)strtoui(buf);
        }
    }

    char *kernel_path = conv_mem_alloc(128);
    if (!config_get_value(kernel_path, 0, 128, "KERNEL_PATH")) {
        panic("KERNEL_PATH not specified");
    }

    struct file_handle *fd = conv_mem_alloc(sizeof(struct file_handle));
    if (fopen(fd, kernel_drive, kernel_part, kernel_path)) {
        panic("Could not open kernel file");
    }

    return (struct kernel_loc) { kernel_drive, kernel_part, kernel_path, fd };
}

// This integer sqrt implementation has been adapted from:
// https://stackoverflow.com/questions/1100090/looking-for-an-efficient-integer-square-root-algorithm-for-arm-thumb2
uint64_t sqrt(uint64_t a_nInput) {
    uint64_t op  = a_nInput;
    uint64_t res = 0;
    uint64_t one = (uint64_t)1 << 62;

    // "one" starts at the highest power of four <= than the argument.
    while (one > op) {
        one >>= 2;
    }

    while (one != 0) {
        if (op >= res + one) {
            op = op - (res + one);
            res = res +  2 * one;
        }
        res >>= 1;
        one >>= 2;
    }

    return res;
}

uint8_t bcd_to_int(uint8_t val) {
    return (val & 0x0f) + ((val & 0xf0) >> 4) * 10;
}

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    asm volatile ("cli" ::: "memory");

    va_list args;

    va_start(args, fmt);

    print("\033[31mPANIC\033[37;1m\033[40m: ");
    vprint(fmt, args);

    va_end(args);

    for (;;) {
        asm volatile ("hlt" ::: "memory");
    }
}

static int char_value(char c) {
    if (c >= 'a' && c <= 'z') {
        return (c - 'a') + 10;
    }
    if (c >= 'A' && c <= 'Z') {
        return (c - 'A') + 10;
    }
    if (c >= '0' && c <= '9'){
        return c - '0';
    }

    return 0;
}

uint64_t strtoui(const char *s) {
    uint64_t n = 0;
    while (*s)
        n = n * 10 + char_value(*(s++));
    return n;
}

uint64_t strtoui16(const char *s) {
    uint64_t n = 0;
    while (*s)
        n = n * 16 + char_value(*(s++));
    return n;
}

int getchar_internal(uint32_t eax) {
    switch ((eax >> 8) & 0xff) {
        case 0x4b:
            return GETCHAR_CURSOR_LEFT;
        case 0x4d:
            return GETCHAR_CURSOR_RIGHT;
        case 0x48:
            return GETCHAR_CURSOR_UP;
        case 0x50:
            return GETCHAR_CURSOR_DOWN;
    }
    return (char)(eax & 0xff);
}

int getchar(void) {
    struct rm_regs r = {0};
    rm_int(0x16, &r, &r);
    return getchar_internal(r.eax);
}
