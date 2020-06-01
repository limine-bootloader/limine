asm (
    ".section .entry\n\t"
    "xor dh, dh\n\t"
    "push edx\n\t"

    // Zero out .bss
    "xor al, al\n\t"
    "lea edi, bss_begin\n\t"
    "lea ecx, bss_end\n\t"
    "lea edx, bss_begin\n\t"
    "sub ecx, edx\n\t"
    "rep stosb\n\t"

    "call main\n\t"
);

#include <drivers/vga_textmode.h>
#include <lib/real.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/part.h>
#include <lib/config.h>
#include <lib/e820.h>
#include <lib/print.h>
#include <fs/file.h>
#include <lib/elf.h>
#include <protos/stivale.h>
#include <protos/linux.h>
#include <protos/chainload.h>

static char *cmdline;
#define CMDLINE_MAX 1024

static char config_entry_name[1024];

void boot_menu(void) {
    text_disable_cursor();
    int selected_entry = 0;

refresh:
    text_clear();
    print("qloader2\n\n");

    print("Select an entry:\n\n");

    int max_entries;
    for (max_entries = 0; ; max_entries++) {
        if (config_get_entry_name(config_entry_name, max_entries, 1024) == -1)
            break;
        if (max_entries == selected_entry)
            print(" -> %s\n", config_entry_name);
        else
            print("    %s\n", config_entry_name);
    }

    if (max_entries == 0)
        panic("Config contains no entries.");

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
                if (!config_get_value(cmdline, 0, CMDLINE_MAX, "KERNEL_CMDLINE")) {
                    if (!config_get_value(cmdline, 0, CMDLINE_MAX, "CMDLINE")) {
                        cmdline[0] = '\0';
                    }
                }
                text_clear();
                return;
            case 'e':
                config_set_entry(selected_entry);
                text_enable_cursor();
                if (!config_get_value(cmdline, 0, CMDLINE_MAX, "KERNEL_CMDLINE")) {
                    if (!config_get_value(cmdline, 0, CMDLINE_MAX, "CMDLINE")) {
                        cmdline[0] = '\0';
                    }
                }
                print("\n\n> ");
                gets(cmdline, cmdline, CMDLINE_MAX);
                text_clear();
                return;
        }
    }
}

void main(int boot_drive) {
    // Initial prompt.
    init_vga_textmode();

    print("qloader2\n\n");

    print("Boot drive: %x\n", boot_drive);

    cmdline = balloc(CMDLINE_MAX);

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

    int timeout; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "TIMEOUT")) {
            timeout = 5;
        } else {
            timeout = (int)strtoui(buf);
        }
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

    if (!config_get_value(cmdline, 0, CMDLINE_MAX, "KERNEL_CMDLINE")) {
        cmdline[0] = '\0';
    }

got_entry:
    init_e820();

    char proto[32];
    if (!config_get_value(proto, 0, 32, "KERNEL_PROTO")) {
        if (!config_get_value(proto, 0, 32, "PROTOCOL")) {
            panic("PROTOCOL not specified");
        }
    }

    if (!strcmp(proto, "stivale")) {
        stivale_load(cmdline, boot_drive);
    } else if (!strcmp(proto, "linux")) {
        linux_load(cmdline, boot_drive);
    } else if (!strcmp(proto, "chainload")) {
        chainload();
    } else {
        panic("Invalid protocol specified");
    }
}
