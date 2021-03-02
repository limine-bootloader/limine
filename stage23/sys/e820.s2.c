#if defined (bios)

#include <stdint.h>
#include <stddef.h>
#include <sys/e820.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <mm/pmm.h>

struct e820_entry_t *e820_map = NULL;
size_t e820_entries;

void init_e820(void) {
    struct rm_regs r = {0};

load_up:
    // Figure out the number of entries
    for (size_t i = 0; ; i++) {
        struct e820_entry_t entry;

        r.eax = 0xe820;
        r.ecx = 24;
        r.edx = 0x534d4150;
        r.edi = (uint32_t)&entry;
        rm_int(0x15, &r, &r);

        if (r.eflags & EFLAGS_CF) {
            e820_entries = i;
            break;
        }

        if (e820_map)
            e820_map[i] = entry;

        if (!r.ebx) {
            e820_entries = ++i;
            break;
        }
    }

    if (e820_map)
        return;

    e820_map = conv_mem_alloc(sizeof(struct e820_entry_t) * e820_entries);
    goto load_up;
}

#endif
