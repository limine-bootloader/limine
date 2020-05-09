#include <stdint.h>
#include <stddef.h>
#include <lib/e820.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/print.h>

struct e820_entry_t *e820_map;
size_t e820_entries;

static const char *e820_type(uint32_t type) {
    switch (type) {
        case 1:
            return "Usable RAM";
        case 2:
            return "Reserved";
        case 3:
            return "ACPI reclaimable";
        case 4:
            return "ACPI NVS";
        case 5:
            return "Bad memory";
        default:
            return "???";
    }
}

void init_e820(void) {
    struct rm_regs r = {0};

    e820_map = balloc(sizeof(struct e820_entry_t));
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

        balloc(sizeof(struct e820_entry_t));
    }

    for (size_t i = 0; i < e820_entries; i++) {
        print("e820: [%X -> %X] : %X  <%s>\n",
              e820_map[i].base,
              e820_map[i].base + e820_map[i].length,
              e820_map[i].length,
              e820_type(e820_map[i].type));
    }
}
