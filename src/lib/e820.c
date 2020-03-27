#include <lib/e820.h>
#include <lib/real.h>
#include <lib/blib.h>

struct e820_entry_t e820_map[E820_MAX_ENTRIES];

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

    for (size_t i = 0; i < E820_MAX_ENTRIES; i++) {
        r.eax = 0xe820;
        r.ecx = 24;
        r.edx = 0x534d4150;
        r.edi = (uint32_t)&e820_map[i];
        rm_int(0x15, &r, &r);

        if (r.eflags & EFLAGS_CF) {
            e820_map[i].type = 0;
            goto done;
        }

        if (!r.ebx) {
            e820_map[i+1].type = 0;
            goto done;
        }
    }

    print("e820: Too many entries!\n");

done:
    for (size_t i = 0; e820_map[i].type; i++) {
        print("e820: [%X -> %X] : %X  <%s>\n",
              e820_map[i].base,
              e820_map[i].base + e820_map[i].length,
              e820_map[i].length,
              e820_type(e820_map[i].type));
    }
}
