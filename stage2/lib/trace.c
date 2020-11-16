#include <stddef.h>
#include <stdint.h>
#include <lib/trace.h>
#include <lib/blib.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <fs/file.h>
#include <mm/pmm.h>

static char *stage2_map = NULL;

void trace_init(void) {
    char map_filename[80];
    if (!config_get_value(NULL, map_filename, 0, 80, "STAGE2_MAP"))
        return;

    struct file_handle stage2_map_file;
    if (!uri_open(&stage2_map_file, map_filename))
        panic("Could not open stage2 map file `%s`", map_filename);

    stage2_map = ext_mem_alloc(stage2_map_file.size);
    fread(&stage2_map_file, stage2_map, 0, stage2_map_file.size);

    print("trace: Stage 2 map file `%s` loaded.\n", map_filename);
}

char *trace_address(size_t *off, size_t addr) {
    if (!stage2_map)
        return NULL;

    uint32_t prev_addr = 0;
    char    *prev_sym  = NULL;

    for (size_t i = 0;;) {
        if (*((uint32_t *)&stage2_map[i]) >= addr) {
            *off = addr - prev_addr;
            return prev_sym;
        }
        prev_addr = *((uint32_t *)&stage2_map[i]);
        i += sizeof(uint32_t);
        prev_sym  = &stage2_map[i];
        while (stage2_map[i++] != 0);
    }
}

void print_stacktrace(size_t *base_ptr) {
    if (!stage2_map)
        print("trace: Symbol names won't be resolved due to missing map file.\n");

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
