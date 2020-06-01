#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdbool.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <drivers/vga_textmode.h>
#include <lib/real.h>
#include <lib/cio.h>
#include <lib/e820.h>
#include <lib/print.h>

// Checks if a given memory range is entirely within a usable e820 entry.
// TODO: Consider the possibility of adjacent usable entries.
// TODO: Consider the possibility of usable entry being overlapped by non-usable
//       ones.
void is_valid_memory_range(size_t base, size_t size) {
    // We don't allow loads below 1MB
    if (base < 0x100000)
        panic("Trying to overwrite bootloader.");
    for (size_t i = 0; i < e820_entries; i++) {
        if (e820_map[i].type != 1)
            continue;
        size_t entry_base = e820_map[i].base;
        size_t entry_top  = e820_map[i].base + e820_map[i].length;
        size_t limit      = base + size;
        if (base  >= entry_base && base  < entry_top &&
            limit >= entry_base && limit < entry_top)
            return;
    }
    panic("Out of memory");
}

uint8_t bcd_to_int(uint8_t val) {
    return (val & 0x0f) + ((val & 0xf0) >> 4) * 10;
}

int cpuid(uint32_t leaf, uint32_t subleaf,
          uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx) {
    uint32_t cpuid_max;
    asm volatile ("cpuid"
                  : "=a" (cpuid_max)
                  : "a" (leaf & 0x80000000)
                  : "rbx", "rcx", "rdx");
    if (leaf > cpuid_max)
        return 1;
    asm volatile ("cpuid"
                  : "=a" (*eax), "=b" (*ebx), "=c" (*ecx), "=d" (*edx)
                  : "a" (leaf), "c" (subleaf));
    return 0;
}

__attribute__((noreturn)) void panic(const char *fmt, ...) {
    asm volatile ("cli");

    va_list args;

    va_start(args, fmt);

    print("PANIC: ");
    vprint(fmt, args);

    va_end(args);

    for (;;) {
        asm volatile ("hlt");
    }
}

extern symbol bss_end;
static size_t bump_allocator_base = (size_t)bss_end;
#define BUMP_ALLOCATOR_LIMIT ((size_t)0x80000)

void brewind(size_t count) {
    bump_allocator_base -= count;
}

void *balloc(size_t count) {
    return balloc_aligned(count, 4);
}

// Only power of 2 alignments
void *balloc_aligned(size_t count, size_t alignment) {
    size_t new_base = bump_allocator_base;
    if (new_base & (alignment - 1)) {
        new_base &= ~(alignment - 1);
        new_base += alignment;
    }
    void *ret = (void *)new_base;
    new_base += count;
    if (new_base >= BUMP_ALLOCATOR_LIMIT)
        panic("Memory allocation failed");
    bump_allocator_base = new_base;
    return ret;
}

__attribute__((used)) static uint32_t int_08_ticks_counter;

__attribute__((naked)) static void int_08_isr(void) {
    asm (
        ".code16\n\t"
        "pushf\n\t"
        "inc dword ptr cs:[int_08_ticks_counter]\n\t"
        "popf\n\t"
        "int 0x40\n\t"   // call callback
        "iret\n\t"
        ".code32\n\t"
    );
}

uint32_t *ivt = 0; // this variable is not static else gcc will optimise the
                   // 0 ptr to a ud2

__attribute__((used)) static void hook_int_08(void) {
    ivt[0x40] = ivt[0x08];  // int 0x40 is callback interrupt
    ivt[0x08] = (uint16_t)(size_t)int_08_isr;
}

__attribute__((used)) static void dehook_int_08(void) {
    ivt[0x08] = ivt[0x40];
}

// This is a dirty hack but we need to execute this full function in real mode
__attribute__((naked))
int pit_sleep_and_quit_on_keypress(uint32_t ticks) {
    asm (
        "call hook_int_08\n\t"

        // pit_ticks in edx
        "mov edx, dword ptr ss:[esp+4]\n\t"

        "lea ecx, int_08_ticks_counter\n\t"

        "mov dword ptr ds:[ecx], 0\n\t"

        // Save non-scratch GPRs
        "push ebx\n\t"
        "push esi\n\t"
        "push edi\n\t"
        "push ebp\n\t"

        // Jump to real mode
        "jmp 0x08:1f\n\t"
        "1: .code16\n\t"
        "mov ax, 0x10\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"
        "mov eax, cr0\n\t"
        "and al, 0xfe\n\t"
        "mov cr0, eax\n\t"
        "jmp 0:2f\n\t"
        "2:\n\t"
        "mov ax, 0\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"

        "sti\n\t"

        "10:\n\t"

        "cmp dword ptr ds:[ecx], edx\n\t"
        "je 30f\n\t" // out on timeout

        "push ecx\n\t"
        "push edx\n\t"
        "mov ah, 0x01\n\t"
        "xor al, al\n\t"
        "int 0x16\n\t"
        "pop edx\n\t"
        "pop ecx\n\t"

        "jz 10b\n\t" // loop

        // on keypress
        "xor ax, ax\n\t"
        "int 0x16\n\t"
        "mov eax, 1\n\t"
        "jmp 20f\n\t"  // out

        "30:\n\t"   // out on timeout
        "xor eax, eax\n\t"

        "20:\n\t"

        "cli\n\t"

        // Jump back to pmode
        "mov ebx, cr0\n\t"
        "or bl, 1\n\t"
        "mov cr0, ebx\n\t"
        "jmp 0x18:4f\n\t"
        "4: .code32\n\t"
        "mov bx, 0x20\n\t"
        "mov ds, bx\n\t"
        "mov es, bx\n\t"
        "mov fs, bx\n\t"
        "mov gs, bx\n\t"
        "mov ss, bx\n\t"

        // Restore non-scratch GPRs
        "pop ebp\n\t"
        "pop edi\n\t"
        "pop esi\n\t"
        "pop ebx\n\t"

        // Exit
        "push eax\n\t"
        "call dehook_int_08\n\t"
        "pop eax\n\t"
        "ret\n\t"
    );
    (void)ticks;
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
    text_get_cursor_pos(&last_x, &last_y);
    text_set_cursor_pos(x, y);
    for (size_t i = 0; i < limit; i++) {
        text_write(" ", 1);
    }
    text_set_cursor_pos(x, y);
    text_write(s, strlen(s));
    text_set_cursor_pos(last_x, last_y);
}

void gets(const char *orig_str, char *buf, size_t limit) {
    size_t orig_str_len = strlen(orig_str);
    memmove(buf, orig_str, orig_str_len);
    buf[orig_str_len] = 0;

    int orig_x, orig_y;
    text_get_cursor_pos(&orig_x, &orig_y);

    print("%s", buf);

    for (size_t i = orig_str_len; ; ) {
        int c = getchar();
        switch (c) {
            case GETCHAR_CURSOR_LEFT:
                if (i) {
                    i--;
                    text_write("\b", 1);
                }
                continue;
            case GETCHAR_CURSOR_RIGHT:
                if (i < strlen(buf)) {
                    i++;
                    text_write(" ", 1);
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
                    text_write("\b", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
                continue;
            case '\r':
                text_write("\n", 1);
                return;
            default:
                if (strlen(buf) < limit-1) {
                    for (size_t j = strlen(buf); ; j--) {
                        buf[j+1] = buf[j];
                        if (j == i)
                            break;
                    }
                    buf[i++] = c;
                    text_write(" ", 1);
                    gets_reprint_string(orig_x, orig_y, buf, limit);
                }
        }
    }
}
