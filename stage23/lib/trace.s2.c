#include <stddef.h>
#include <stdint.h>
#include <lib/trace.h>
#include <lib/blib.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <fs/file.h>
#include <mm/pmm.h>

extern symbol limine_map;

char *trace_address(size_t *off, size_t addr) {
    if (!stage3_loaded)
        return NULL;

    uint32_t prev_addr = 0;
    char    *prev_sym  = NULL;

    for (size_t i = 0;;) {
        if (*((uint32_t *)&limine_map[i]) >= addr) {
            *off = addr - prev_addr;
            return prev_sym;
        }
        prev_addr = *((uint32_t *)&limine_map[i]);
        i += sizeof(uint32_t);
        prev_sym  = &limine_map[i];
        while (limine_map[i++] != 0);
    }
}

void print_stacktrace(size_t *base_ptr) {
    if (!stage3_loaded) {
        print("trace: Stack trace omitted because stage 3 was not loaded yet.\n");
        return;
    }

    if (base_ptr == NULL) {
        asm volatile (
            "mov %0, ebp"
            : "=g"(base_ptr)
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
            print("  [%x] <%s+%x>\n", ret_addr, name, off);
        else
            print("  [%x]\n", ret_addr);
        if (!old_bp)
            break;
        base_ptr = (void*)old_bp;
    }
    print("End of trace.\n");
}
