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
#include <protos/stivale.h>
#include <protos/stivale2.h>
#include <protos/linux.h>
#include <protos/chainload.h>
#include <menu.h>
#include <pxe/pxe.h>
#include <pxe/tftp.h>
#include <drivers/disk.h>
#include <sys/idt.h>

extern uint64_t stage3_build_id;

struct volume *boot_volume;

#if bios == 1

bool stage3_loaded = false;
static bool stage3_found = false;

extern symbol stage3_addr;
extern symbol limine_sys_size;

static bool stage3_init(struct volume *part) {
    struct file_handle stage3;

    if (fopen(&stage3, part, "/limine.sys")
     && fopen(&stage3, part, "/boot/limine.sys")) {
        return false;
    }

    stage3_found = true;

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

enum {
	BOOTED_FROM_HDD,
	BOOTED_FROM_PXE,
	BOOTED_FROM_CD
};

__attribute__((noreturn))
void entry(uint8_t boot_drive, int boot_from) {
    // XXX DO NOT MOVE A20 ENABLE CALL
    if (!a20_enable())
        panic("Could not enable A20 line");

    init_e820();
    init_memmap();

    term_textmode();

    copyright_notice();

    init_idt();

    disk_create_index();

    if (boot_from == BOOTED_FROM_HDD || boot_from == BOOTED_FROM_CD) {
        boot_volume = volume_get_by_bios_drive(boot_drive);
    } else if (boot_from == BOOTED_FROM_PXE) {
        pxe_init();
        boot_volume = pxe_bind_volume();
    }

    volume_iterate_parts(boot_volume,
        if (stage3_init(_PART)) {
            break;
        }
    );

    if (!stage3_found)
        print("\n"
              "!! Stage 3 file not found!\n"
              "!! Have you copied limine.sys to the root or /boot directories of\n"
              "!! one of the partitions on the boot device?\n\n");

    if (!stage3_loaded)
        panic("Failed to load stage 3.");

    __attribute__((noreturn))
    void (*stage3)(int boot_from) = (void *)stage3_addr;

    stage3(boot_from);
}

#endif
