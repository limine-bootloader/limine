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
#include <lib/part.h>
#include <lib/config.h>
#include <fs/file.h>
#include <sys/interrupt.h>
#include <lib/elf.h>
#include <protos/stivale.h>

extern symbol bss_begin;
extern symbol bss_end;

void main(int boot_drive) {
    struct file_handle f;

    // Zero out .bss section
    for (uint8_t *p = bss_begin; p < bss_end; p++)
        *p = 0;

    // Initial prompt.
    init_vga_textmode();

    init_idt();

    print("qLoader 2\n\n");
    print("Boot drive: %x\n", boot_drive);

    // Look for config file.
    print("Searching for config file...\n");
    struct part parts[4];
    for (int i = 0; ; i++) {
        if (i == 4) {
            panic("Config file not found.");
        }
        print("Checking partition %d...\n", i);
        int ret = get_part(&parts[i], boot_drive, i);
        if (ret) {
            print("Partition not found.\n");
        } else {
            print("Partition found.\n");
            if (!init_config(boot_drive, i)) {
                print("Config file found and loaded.\n");
                break;
            }
        }
    }

    int drive, part, timeout;
    char path[128], cmdline[128], proto[64], buf[32];

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

    print("\n\n");
    for (int i = timeout; i; i--) {
        print("\rBooting in %d (press any key to edit command line)...", i);
        if (pit_sleep_and_quit_on_keypress(18)) {
            print("\n\n> ");
            gets(cmdline, cmdline, 128);
            break;
        }
    }
    print("\n\n");

    fopen(&f, drive, part, path);

    if (!strcmp(proto, "stivale")) {
        stivale_load(&f, cmdline);
    } else if (!strcmp(proto, "qword")) {
        fread(&f, (void *)0x100000, 0, f.size);
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
