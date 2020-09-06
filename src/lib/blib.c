#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/term.h>
#include <lib/real.h>
#include <lib/cio.h>
#include <lib/e820.h>
#include <lib/print.h>
#include <lib/asm.h>

uint8_t bcd_to_int(uint8_t val) {
    return (val & 0x0f) + ((val & 0xf0) >> 4) * 10;
}

int cpuid(uint32_t leaf, uint32_t subleaf,
          uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    uint32_t cpuid_max;
    ASM("cpuid\n\t", "=a" (cpuid_max)
                   : "a" (leaf & 0x80000000)
                   : "ebx", "ecx", "edx");
    if (leaf > cpuid_max)
        return 1;
    ASM("cpuid\n\t", "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                   : "a" (leaf), "c" (subleaf));
    return 0;
}

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    ASM("cli\n\t", :: "memory");

    va_list args;

    va_start(args, fmt);

    print("\033[31mPANIC\033[37;1m\033[40m: ");
    vprint(fmt, args);

    va_end(args);

    for (;;) {
        ASM("hlt\n\t", :: "memory");
    }
}

extern symbol bss_end;
static size_t bump_allocator_base = (size_t)bss_end;
#define BUMP_ALLOCATOR_LIMIT ((size_t)0x7ff00)

void brewind(size_t count) {
    bump_allocator_base -= count;
}

void *balloc(size_t count) {
    return balloc_aligned(count, 4);
}

// Only power of 2 alignments
void *balloc_aligned(size_t count, size_t alignment) {
    size_t new_base = ALIGN_UP(bump_allocator_base, alignment);
    void *ret = (void *)new_base;
    new_base += count;
    if (new_base >= BUMP_ALLOCATOR_LIMIT)
        panic("Memory allocation failed");
    bump_allocator_base = new_base;
    return ret;
}

uint64_t strtoui(const char *s) {
    uint64_t n = 0;
    while (*s)
        n = n * 10 + ((*(s++)) - '0');
    return n;
}

int getchar(void) {
    struct rm_regs r = {0};
    rm_int(0x16, &r, &r);
    switch ((r.eax >> 8) & 0xff) {
        case 0x4b:
            return GETCHAR_CURSOR_LEFT;
        case 0x4d:
            return GETCHAR_CURSOR_RIGHT;
        case 0x48:
            return GETCHAR_CURSOR_UP;
        case 0x50:
            return GETCHAR_CURSOR_DOWN;
    }
    return (char)(r.eax & 0xff);
}

static void gets_reprint_string(int x, int y, const char *s, size_t limit) {
    int last_x, last_y;
    get_cursor_pos(&last_x, &last_y);
    set_cursor_pos(x, y);
    for (size_t i = 0; i < limit; i++) {
        term_write(" ", 1);
    }
    set_cursor_pos(x, y);
    term_write(s, strlen(s));
    set_cursor_pos(last_x, last_y);
}

void gets(const char *orig_str, char *buf, size_t limit) {
    size_t orig_str_len = strlen(orig_str);
    memmove(buf, orig_str, orig_str_len);
    buf[orig_str_len] = 0;

    int orig_x, orig_y;
    get_cursor_pos(&orig_x, &orig_y);

    print("%s", buf);

    for (size_t i = orig_str_len; ; ) {
        int c = getchar();
        switch (c) {
            case GETCHAR_CURSOR_LEFT:
                if (i) {
                    i--;
                    term_write("\b", 1);
                }
                continue;
            case GETCHAR_CURSOR_RIGHT:
                if (i < strlen(buf)) {
                    i++;
                    term_write(" ", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
                continue;
            case '\b':
                if (i) {
                    i--;
                    for (size_t j = i; ; j++) {
                        buf[j] = buf[j+1];
                        if (!buf[j])
                            break;
                    }
                    term_write("\b", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
                continue;
            case '\r':
                term_write("\n", 1);
                return;
            default:
                if (strlen(buf) < limit-1) {
                    for (size_t j = strlen(buf); ; j--) {
                        buf[j+1] = buf[j];
                        if (j == i)
                            break;
                    }
                    buf[i++] = c;
                    term_write(" ", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
        }
    }
}
