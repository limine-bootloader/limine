asm (
    ".section .entry\n\t"
    "xor dh, dh\n\t"
    "push edx\n\t"
    "call main\n\t"
);

#include <drivers/vga_textmode.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/mbr.h>
#include <lib/config.h>
#include <fs/echfs.h>
#include <sys/interrupt.h>

#define CONFIG_NAME "qloader2.cfg"

extern symbol bss_begin;
extern symbol bss_end;

static int config_loaded = 0;

void main(int boot_drive) {
    // Zero out .bss section
    for (uint8_t *p = bss_begin; p < bss_end; p++)
        *p = 0;

    // Initial prompt.
    init_vga_textmode();

    init_idt();

    print("qLoader 2\n\n");
    print("=> Boot drive: %x\n", boot_drive);

    // Enumerate partitions.
    struct mbr_part parts[4];
    for (int i = 0; i < 4; i++) {
        print("=> Checking for partition %d...\n", i);
        int ret = mbr_get_part(&parts[i], boot_drive, i);
        if (ret) {
            print("   Not found!\n");
        } else {
            print("   Found!\n");
            if (!config_loaded) {
                if (!load_echfs_file(boot_drive, i, (void *)0x100000, CONFIG_NAME)) {
                    config_loaded = 1;
                    print("   Config file found and loaded!\n");
                }
            }
        }
    }

    int drive, part;
    char path[128], cmdline[128];

    if (config_loaded) {
        char buf[32];
        config_get_value(buf, 32, (void*)0x100000, "KERNEL_DRIVE");
        drive = (int)strtoui(buf);
        config_get_value(buf, 32, (void*)0x100000, "KERNEL_PARTITION");
        part = (int)strtoui(buf);
        config_get_value(path, 128, (void*)0x100000, "KERNEL_PATH");
        config_get_value(cmdline, 128, (void*)0x100000, "KERNEL_CMDLINE");
    } else {
        print("   !! NO CONFIG FILE FOUND ON BOOT DRIVE !!");
        for (;;);
    }

    print("\n");
    for (int i = 3; i; i--) {
        print("\rBooting in %d (press any key to edit command line)...", i);
        if (pit_sleep_and_quit_on_keypress(18)) {
            print("\n\n> ");
            gets(cmdline, cmdline, 128);
            break;
        }
    }

    load_echfs_file(drive, part, (void *)0x100000, path);

    // Boot the kernel.
    asm volatile (
        "cli\n\t"
        "jmp 0x100000\n\t"
        :
        : "b" (cmdline)
        : "memory"
    );
}
