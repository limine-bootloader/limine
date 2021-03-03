#if defined (bios)

#include <stddef.h>
#include <stdint.h>
#include <protos/chainload.h>
#include <lib/part.h>
#include <lib/config.h>
#include <lib/blib.h>
#include <drivers/disk.h>
#include <lib/term.h>
#include <mm/mtrr.h>

__attribute__((section(".realmode"), used))
static void spinup(uint8_t drive) {
    asm volatile (
        "cld\n\t"

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
        "jmp 0x0000:1f\n\t"
        "1:\n\t"
        "mov ax, 0\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"

        "sti\n\t"

        "push 0\n\t"
        "push 0x7c00\n\t"
        "retf\n\t"

        ".code32\n\t"
        :
        : "d" (drive)
        : "memory"
    );
}

void chainload(char *config) {
    uint64_t val;

    int part; {
        char *part_config = config_get_value(config, 0, "PARTITION");
        if (part_config == NULL) {
            part = -1;
        } else {
            val = strtoui(part_config, NULL, 10);
            if (val < 1 || val > 256) {
                panic("BIOS partition number outside range 1-256");
            }
            part = val - 1;
        }
    }
    int drive; {
        char *drive_config = config_get_value(config, 0, "DRIVE");
        if (drive_config == NULL) {
            panic("DRIVE not specified");
        }
        val = strtoui(drive_config, NULL, 10);
        if (val < 1 || val > 16) {
            panic("BIOS drive number outside range 1-16");
        }
        drive = (val - 1) + 0x80;
    }

    term_deinit();

    struct volume p = {0};
    volume_get_by_coord(&p, drive, part);

    volume_read(&p, (void *)0x7c00, 0, 512);

    mtrr_restore();

    spinup(drive);
}

#endif
