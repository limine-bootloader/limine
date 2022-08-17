#include <stddef.h>
#include <stdint.h>
#include <lib/trace.h>
#include <lib/blib.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <fs/file.h>
#include <mm/pmm.h>

#if bios == 1
extern symbol stage2_map;
#elif uefi == 1
extern symbol __image_base;
#endif

extern symbol full_map;

static char *trace_address(size_t *off, size_t addr) {
    char *limine_map;

#if bios == 1
    if (!stage3_loaded)
        limine_map = stage2_map;
    else
        limine_map = full_map;
#elif uefi == 1
    limine_map = full_map;

    addr -= (size_t)__image_base;
#endif

    uintptr_t prev_addr = 0;
    char     *prev_sym  = NULL;

    for (size_t i = 0;;) {
        if (*((uintptr_t *)&limine_map[i]) >= addr) {
            *off = addr - prev_addr;
            return prev_sym;
        }
        prev_addr = *((uintptr_t *)&limine_map[i]);
        i += sizeof(uintptr_t);
        prev_sym  = &limine_map[i];
        while (limine_map[i++] != 0);
    }
}

void print_stacktrace(size_t *base_ptr) {
    if (base_ptr == NULL) {
        asm volatile (
#if defined (__i386__)
            "movl %%ebp, %0"
#elif defined (__x86_64__)
            "movq %%rbp, %0"
#elif defined (__aarch64__)
            "mov %0, x29"
#endif
            : "=r"(base_ptr)
            :: "memory"
        );
    }
    print("Stacktrace:\n");
    for (;;) {
        size_t old_bp = base_ptr[0];
        size_t ret_addr = base_ptr[1];
        if (!ret_addr)
            break;
        size_t off;
        char *name = trace_address(&off, ret_addr);
        if (name)
            print("  [%p] <%s+%p>\n", ret_addr, name, off);
        else
            print("  [%p]\n", ret_addr);
        if (!old_bp)
            break;
        base_ptr = (void*)old_bp;
    }
    print("End of trace. ");
}
