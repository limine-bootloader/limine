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
#include <protos/linux.h>

static char cmdline[128];

void boot_menu(void) {
    text_disable_cursor();
    int selected_entry = 0;

refresh:
    text_clear();
    print("qloader2\n\n");

    print("Select an entry:\n\n");

    size_t max_entries;
    for (max_entries = 0; ; max_entries++) {
        char buf[32];
        if (config_get_entry_name(buf, max_entries, 32) == -1)
            break;
        if (max_entries == selected_entry)
            print(" -> %s\n", buf);
        else
            print("    %s\n", buf);
    }

    print("\nArrows to choose, enter to select, 'e' to edit command line.");

    for (;;) {
        int c = getchar();
        switch (c) {
            case GETCHAR_CURSOR_UP:
                if (--selected_entry == -1)
                    selected_entry = max_entries - 1;
                goto refresh;
            case GETCHAR_CURSOR_DOWN:
                if (++selected_entry == max_entries)
                    selected_entry = 0;
                goto refresh;
            case '\r':
                config_set_entry(selected_entry);
                text_enable_cursor();
                text_clear();
                return;
            case 'e':
                config_set_entry(selected_entry);
                text_enable_cursor();
                config_get_value(cmdline, 0, 128, "KERNEL_CMDLINE");
                print("\n\n> ");
                gets(cmdline, cmdline, 128);
                text_clear();
                return;
        }
    }
}

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

    print("qloader2\n\n");
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

    int drive, part;
    char path[128], proto[64], buf[32];

    int timeout;
    if (!config_get_value(buf, 0, 64, "TIMEOUT")) {
        timeout = 5;
    } else {
        timeout = (int)strtoui(buf);
    }

    print("\n");
    for (int i = timeout; i; i--) {
        print("\rBooting in %d (press any key for boot menu, command line edit)...", i);
        if (pit_sleep_and_quit_on_keypress(18)) {
            boot_menu();
            goto got_entry;
        }
    }
    print("\n\n");

    if (config_set_entry(0) == -1) {
        panic("Invalid config entry.");
    }

got_entry:
    if (!config_get_value(buf, 0, 32, "KERNEL_DRIVE")) {
        print("KERNEL_DRIVE not specified, using boot drive (%x)", boot_drive);
        drive = boot_drive;
    } else {
        drive = (int)strtoui(buf);
    }
    config_get_value(buf, 0, 32, "KERNEL_PARTITION");
    part = (int)strtoui(buf);
    config_get_value(path, 0, 128, "KERNEL_PATH");
    config_get_value(proto, 0, 64, "KERNEL_PROTO");

    fopen(&f, drive, part, path);

    if (!strcmp(proto, "stivale")) {
        stivale_load(&f, cmdline);
    } else if (!strcmp(proto, "linux")) {
        linux_load(&f, cmdline);
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
