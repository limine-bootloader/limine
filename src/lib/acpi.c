#include <stddef.h>
#include <lib/acpi.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/print.h>

void *get_rsdp(void) {
    for (size_t i = 0x80000; i < 0x100000; i += 16) {
        if (i == 0xa0000) {
            /* skip video mem and mapped hardware */
            i = 0xe0000 - 16;
            continue;
        }
        if (!strncmp((char *)i, "RSD PTR ", 8)) {
            print("acpi: Found RSDP at %x\n", i);
            return (void *)i;
        }
    }

    return NULL;
}
