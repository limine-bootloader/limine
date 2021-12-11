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
#include <sys/cpu.h>

struct volume *boot_volume;

#if bios == 1

bool stage3_loaded = false;
static bool stage3_found = false;

extern symbol stage3_addr;
extern symbol limine_sys_size;
extern symbol build_id_s2;
extern symbol build_id_s3;

static bool stage3_init(struct volume *part) {
    struct file_handle *stage3;

    if ((stage3 = fopen(part, "/limine.sys")) == NULL
     && (stage3 = fopen(part, "/boot/limine.sys")) == NULL) {
        return false;
    }

    stage3_found = true;

    if (stage3->size != (size_t)limine_sys_size) {
        term_textmode();
        print("limine.sys size incorrect.\n");
        return false;
    }

    fread(stage3, stage3_addr,
          (uintptr_t)stage3_addr - 0x8000,
          stage3->size - ((uintptr_t)stage3_addr - 0x8000));

    fclose(stage3);

    if (memcmp(build_id_s2 + 16, build_id_s3 + 16, 20) != 0) {
        term_textmode();
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
        panic(false, "Could not enable A20 line");

    term_notready();

    {
    struct rm_regs r = {0};
    r.eax = 0x0003;
    rm_int(0x10, &r, &r);

    current_video_mode = -1;

    outb(0x3d4, 0x0a);
    outb(0x3d5, 0x20);
    }

    init_e820();
    init_memmap();

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

    if (!stage3_found) {
        term_textmode();
        print("\n"
              "!! Stage 3 file not found!\n"
              "!! Have you copied limine.sys to the root or /boot directories of\n"
              "!! one of the partitions on the boot device?\n\n");
    }

    if (!stage3_loaded) {
        panic(false, "Failed to load stage 3.");
    }

    stage3_common();
}

#endif
