asm (
    ".section .entry\n\t"
    "xor dh, dh\n\t"
    "push edx\n\t"
    "call main\n\t"
);

#include <drivers/vga_textmode.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/mbr.h>
#include <lib/config.h>
#include <sys/interrupt.h>
#include <lib/elf.h>
#include <protos/stivale.h>
#include <fs/ext2fs.h>

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

            print("   => Checking for EXT2FS\n");
            init_ext2(boot_drive, parts[i]);

            print("   => Checking for ECHFS\n");
            int ret = is_echfs(boot_drive, parts[i]);
            if (ret == 0) {
                print("      ECHFS Found!\n");
            } else {
                print("      ECHFS Not found!\n");
            }

            if (!config_loaded) {
                if (!init_config(boot_drive, parts[i])) {
                    config_loaded = 1;
                    print("   Config file found and loaded!\n");
                }
            }
        }
    }

    int drive, part, timeout;
    char path[128], cmdline[128], proto[64];

    if (config_loaded) {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "KERNEL_DRIVE")) {
            print("KERNEL_DRIVE not specified, using boot drive (%x)", boot_drive);
            drive = boot_drive;
        } else {
            drive = (int)strtoui(buf);
        }
        if (!config_get_value(buf, 0, 64, "TIMEOUT")) {
            timeout = 5;
        } else {
            timeout = (int)strtoui(buf);
        }
        config_get_value(buf, 0, 32, "KERNEL_PARTITION");
        part = (int)strtoui(buf);
        config_get_value(path, 0, 128, "KERNEL_PATH");
        config_get_value(cmdline, 0, 128, "KERNEL_CMDLINE");
        config_get_value(proto, 0, 64, "KERNEL_PROTO");
    } else {
        print("   !! NO CONFIG FILE FOUND ON BOOT DRIVE !!\n");
        for (;;);
    }

    print("\n");
    for (int i = timeout; i; i--) {
        print("\rBooting in %d (press any key to edit command line)...", i);
        if (pit_sleep_and_quit_on_keypress(18)) {
            print("\n\n> ");
            gets(cmdline, cmdline, 128);
            break;
        }
    }
    print("\n");

    // TODO: edit stivale
    FILE *file = bfopen(path, drive, parts[part]);

    if (!strcmp(proto, "stivale")) {
        stivale_load(file, cmdline);
    } else if (!strcmp(proto, "qword")) {
        // TODO: GRAB ENTRY SIZE
        bfgets((void *)0x100000, 0, 100000, file);
        //echfs_read(&f, (void *)0x100000, 0, f.dir_entry.size);
        // Boot the kernel.
        asm volatile (
            "cli\n\t"
            "jmp 0x100000\n\t"
            :
            : "b" (cmdline)
            : "memory"
        );
    } else {
        print("Invalid protocol specified: `%s`.\n", proto);
        for (;;);
    }
}