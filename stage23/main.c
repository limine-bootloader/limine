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

enum {
	BOOT_FROM_HDD,
	BOOT_FROM_PXE,
	BOOT_FROM_CD
};

__attribute__((noreturn))
void entry(uint8_t _boot_drive, int boot_from) {
    boot_drive = _boot_drive;

    booted_from_pxe = (boot_from == BOOT_FROM_PXE);
    booted_from_cd = (boot_from == BOOT_FROM_CD);

    mtrr_save();

    term_textmode();

    print("Limine " LIMINE_VERSION "\n\n");

    if (!a20_enable())
        panic("Could not enable A20 line");

    init_e820();
    init_memmap();

    volume_create_index();

    switch (boot_from) {
    case BOOT_FROM_HDD: {
        print("Boot drive: %x\n", boot_drive);
        struct volume boot_volume;
        volume_get_by_coord(&boot_volume, boot_drive, -1);
        struct volume part = boot_volume;
        bool stage3_loaded = false, config_loaded = false;
        for (int i = 0; ; i++) {
            if (!stage3_loaded && stage3_init(&part)) {
                stage3_loaded = true;
                print("Stage 3 found and loaded.\n");
            }
            if (!config_loaded && !init_config_disk(&part)) {
                config_loaded = true;
                print("Config file found and loaded.\n");
                boot_partition = i - 1;
            }
            int ret = part_get(&part, &boot_volume, i);
            switch (ret) {
                case INVALID_TABLE:
                case END_OF_TABLE:
                    goto break2;
            }
        }
break2:
        if (!stage3_loaded)
            panic("Stage 3 not loaded.");
        if (!config_loaded)
            panic("Config file not found.");
        break;
    }

    case BOOT_FROM_PXE:
        pxe_init();
        if (init_config_pxe()) {
            panic("Failed to load config file");
        }
        print("Config loaded via PXE\n");
        break;
    }

    stage3();
}

__attribute__((noreturn))
__attribute__((section(".stage3_entry")))
void stage3_entry(void) {
    char *cmdline;
    char *config = menu(&cmdline);

    char *proto = config_get_value(config, 0, "PROTOCOL");
    if (proto == NULL) {
        panic("PROTOCOL not specified");
    }

    if (!strcmp(proto, "stivale")) {
        stivale_load(config, cmdline);
    } else if (!strcmp(proto, "stivale2")) {
        stivale2_load(config, cmdline, booted_from_pxe);
    } else if (!strcmp(proto, "linux")) {
        linux_load(config, cmdline);
    } else if (!strcmp(proto, "chainload")) {
        chainload(config);
    } else {
        panic("Invalid protocol specified");
    }

    for (;;);
}
