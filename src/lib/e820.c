#include <lib/e820.h>
#include <lib/real.h>
#include <lib/blib.h>

struct e820_entry_t *e820_map;

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

int init_e820(void) {
    struct rm_regs r = {0};

    int entry_count;

    e820_map = balloc(sizeof(struct e820_entry_t));
    for (int i = 0; ; i++) {
        struct e820_entry_t entry;

        r.eax = 0xe820;
        r.ecx = 24;
        r.edx = 0x534d4150;
        r.edi = (uint32_t)&entry;
        rm_int(0x15, &r, &r);

        e820_map[i] = entry;

        if (r.eflags & EFLAGS_CF) {
            entry_count = i;
            break;
        }

        if (!r.ebx) {
            entry_count = ++i;
            break;
        }

        balloc(sizeof(struct e820_entry_t));
    }

    for (int i = 0; i < entry_count; i++) {
        print("e820: [%X -> %X] : %X  <%s>\n",
              e820_map[i].base,
              e820_map[i].base + e820_map[i].length,
              e820_map[i].length,
              e820_type(e820_map[i].type));
    }

    return entry_count;
}
