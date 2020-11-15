#include <limine.h>
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

void entry(uint8_t _boot_drive, int pxe_boot) {
    boot_drive = _boot_drive;

    mtrr_save();

    term_textmode();

    print("Limine " LIMINE_VERSION "\n\n");

    if (!a20_enable())
        panic("Could not enable A20 line");

    part_create_index();
    init_e820();
    init_memmap();

    if (pxe_boot) {
        pxe_init();
        if(init_config_pxe()) {
            panic("failed to load config file");
        }
        print("config loaded");
    } else {
        print("Boot drive: %x\n", boot_drive);
        // Look for config file.
        print("Searching for config file...\n");
        for (int i = 0; ; i++) {
            struct part part;
            print("Checking partition %d...\n", i);
            int ret = part_get(&part, boot_drive, i);
            switch (ret) {
                case INVALID_TABLE:
                    panic("Partition table of boot drive is invalid.");
                case END_OF_TABLE:
                    panic("Config file not found.");
                case NO_PARTITION:
                    print("Partition not found.\n");
                    continue;
            }
            print("Partition found.\n");
            if (!init_config_disk(&part)) {
                print("Config file found and loaded.\n");
                break;
            }
        }
    }

    trace_init();

    char *cmdline = menu();

    char proto[32];
    if (!config_get_value(proto, 0, 32, "PROTOCOL")) {
        panic("PROTOCOL not specified");
    }

    if (!strcmp(proto, "stivale")) {
        stivale_load(cmdline);
    } else if (!strcmp(proto, "stivale2")) {
        stivale2_load(cmdline);
    } else if (!strcmp(proto, "linux")) {
        linux_load(cmdline);
    } else if (!strcmp(proto, "chainload")) {
        chainload();
    } else {
        panic("Invalid protocol specified");
    }
}
