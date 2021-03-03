#include <lib/term.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/part.h>
#include <lib/config.h>
#include <lib/trace.h>
#include <sys/e820.h>
#include <sys/a20.h>
#include <lib/print.h>
#include <fs/file.h>
#include <lib/elf.h>
#include <mm/pmm.h>
#include <mm/mtrr.h>
#include <protos/stivale.h>
#include <protos/stivale2.h>
#include <protos/linux.h>
#include <protos/chainload.h>
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>

extern uint64_t stage3_build_id;

uint8_t boot_drive;
int     boot_partition = -1;

bool booted_from_pxe = false;
bool booted_from_cd = false;
bool stage3_loaded = false;

extern symbol stage3_addr;
extern symbol limine_sys_size;

static bool stage3_init(struct volume *part) {
    struct file_handle stage3;

    if (fopen(&stage3, part, "/limine.sys")
     && fopen(&stage3, part, "/boot/limine.sys")) {
        return false;
    }

    if (stage3.size != (size_t)limine_sys_size) {
        print("limine.sys size incorrect.\n");
        return false;
    }

    fread(&stage3, stage3_addr,
          (uintptr_t)stage3_addr - 0x8000,
          stage3.size - ((uintptr_t)stage3_addr - 0x8000));

    if (BUILD_ID != stage3_build_id) {
        print("limine.sys build ID mismatch.\n");
        return false;
    }

    stage3_loaded = true;

    return true;
}

__attribute__((noreturn))
void entry(uint8_t _boot_drive, int boot_from) {
    boot_drive = _boot_drive;

    booted_from_pxe = (boot_from == BOOT_FROM_PXE);
    booted_from_cd = (boot_from == BOOT_FROM_CD);

    term_textmode();

    print("Limine " LIMINE_VERSION "\n\n");

    if (!a20_enable())
        panic("Could not enable A20 line");

    init_e820();
    init_memmap();

    volume_create_index();

    switch (boot_from) {
        case BOOT_FROM_HDD:
        case BOOT_FROM_CD: {
            struct volume boot_volume = {0};
            volume_get_by_coord(&boot_volume, boot_drive, -1);
            struct volume part = boot_volume;
            for (int i = 0; ; i++) {
                if (stage3_init(&part)) {
                    print("Stage 3 found and loaded.\n");
                    break;
                }
                int ret = part_get(&part, &boot_volume, i);
                switch (ret) {
                    case INVALID_TABLE:
                    case END_OF_TABLE:
                        panic("Stage 3 not found.");
                }
            }
            break;
        }
    }

    __attribute__((noreturn))
    void (*stage3)(int boot_from) = (void *)stage3_addr;

    stage3(boot_from);
}
