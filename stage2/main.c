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
#include <lib/tinf.h>

enum {
	BOOT_FROM_HDD,
	BOOT_FROM_PXE,
	BOOT_FROM_CD
};

void entry(uint8_t _boot_drive, int boot_from, void *_tinf_gzip_uncompress) {
    boot_drive = _boot_drive;
    tinf_gzip_uncompress = _tinf_gzip_uncompress;

    booted_from_pxe = (boot_from == BOOT_FROM_PXE);
    booted_from_cd = (boot_from == BOOT_FROM_CD);

    mtrr_save();

    term_textmode();

    print("Limine " LIMINE_VERSION "\n\n");

    if (!a20_enable())
        panic("Could not enable A20 line");

    init_e820();
    init_memmap();

    struct volume part;
    switch(boot_from) {
    case BOOT_FROM_HDD:
        volume_create_index();
        print("Boot drive: %x\n", boot_drive);
        // Look for config file.
        print("Searching for config file...\n");
        for (int i = 0; ; i++) {
            int ret = volume_get_by_coord(&part, boot_drive, i);
            switch (ret) {
                case INVALID_TABLE:
                    panic("Partition table of boot drive is invalid.");
                case END_OF_TABLE:
                    panic("Config file not found.");
                case NO_PARTITION:
                    continue;
            }
            if (!init_config_disk(&part)) {
                print("Config file found and loaded.\n");
                boot_partition = i;
                break;
            }
        }
        break;

    case BOOT_FROM_PXE:
        volume_create_index();
        pxe_init();
        if (init_config_pxe()) {
            panic("Failed to load config file");
        }
        print("Config loaded via PXE\n");
        break;

    case BOOT_FROM_CD:
        // Gotta do some minor hacks, a CD has no partitions :^)
        part.drive = boot_drive;
        part.sector_size = 2048;
        part.first_sect = 0;

        if(init_config_disk(&part))
            panic("Failed to load config file");
        break;
    }

    trace_init();

    char *cmdline;
    char *config = menu(&cmdline);

    char *proto = config_get_value(config, 0, "PROTOCOL");
    if (proto == NULL) {
        panic("PROTOCOL not specified");
    }

    if (!strcmp(proto, "stivale")) {
        stivale_load(config, cmdline);
    } else if (!strcmp(proto, "stivale2")) {
        stivale2_load(config, cmdline, boot_from);
    } else if (!strcmp(proto, "linux")) {
        linux_load(config, cmdline);
    } else if (!strcmp(proto, "chainload")) {
        chainload(config);
    } else {
        panic("Invalid protocol specified");
    }
}
