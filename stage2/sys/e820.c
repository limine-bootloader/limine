#include <stdint.h>
#include <stddef.h>
#include <sys/e820.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <mm/pmm.h>

struct e820_entry_t *e820_map;
size_t e820_entries;

void init_e820(void) {
    struct rm_regs r = {0};

    e820_map = conv_mem_alloc(sizeof(struct e820_entry_t));
    for (size_t i = 0; ; i++) {
        struct e820_entry_t entry;

        r.eax = 0xe820;
        r.ecx = 24;
        r.edx = 0x534d4150;
        r.edi = (uint32_t)&entry;
        rm_int(0x15, &r, &r);

        e820_map[i] = entry;

        if (r.eflags & EFLAGS_CF) {
            e820_entries = i;
            break;
        }

        if (!r.ebx) {
            e820_entries = ++i;
            break;
        }

        conv_mem_alloc(sizeof(struct e820_entry_t));
    }

    print("E820 memory map:\n");
    print_memmap(e820_map, e820_entries);
}
