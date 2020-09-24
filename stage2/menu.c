#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <menu.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/config.h>
#include <lib/term.h>
#include <mm/pmm.h>
#include <drivers/vbe.h>

static char *cmdline;
#define CMDLINE_MAX 1024

static char config_entry_name[1024];

void load_color_from_config(void) {
    char buf[16];

    uint32_t colorsheme[9] = {
        0x00191919, // black
        0x00aa0000, // red
        0x0000aa00, // green
        0x00aa5500, // brown
        0x000000aa, // blue
        0x009076DE, // magenta
        0x0000aaaa, // cyan
        0x00aaaaaa, // grey
        0x00ffffff, // white
    };

    if (config_get_value(buf, 0, 16, "COLORSCHEME_BLACK")) {
        colorsheme[0] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_RED")) {
        colorsheme[1] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_GREEN")) {
        colorsheme[2] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_BROWN")) {
        colorsheme[3] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_BLUE")) {
        colorsheme[4] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_MAGENTA")) {
        colorsheme[5] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_CYAN")) {
        colorsheme[6] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_GREY")) {
        colorsheme[7] = (int)strtoui16(buf);
    }

    if (config_get_value(buf, 0, 16, "COLORSCHEME_WHITE")) {
        colorsheme[8] = (int)strtoui16(buf);
    }

    vge_set_colors(colorsheme);
}

char *menu(int boot_drive) {
    cmdline = conv_mem_alloc(CMDLINE_MAX);

    char buf[16];

    // If there is no TEXTMODE config key or the value is not "on", enable graphics
    if (config_get_value(buf, 0, 16, "TEXTMODE") == NULL || strcmp(buf, "on")) {
        load_color_from_config();

        int bg_drive;
        if (!config_get_value(buf, 0, 16, "BACKGROUND_DRIVE")) {
            bg_drive = boot_drive;
        } else {
            bg_drive = (int)strtoui(buf);
        }
        int bg_part;
        if (!config_get_value(buf, 0, 16, "BACKGROUND_PARTITION")) {
            goto nobg;
        } else {
            bg_part = (int)strtoui(buf);
        }
        if (!config_get_value(cmdline, 0, CMDLINE_MAX, "BACKGROUND_PATH"))
            goto nobg;

        struct file_handle *bg_file = conv_mem_alloc(sizeof(struct file_handle));
        if (fopen(bg_file, bg_drive, bg_part, cmdline))
            goto nobg;

        struct image *bg = conv_mem_alloc(sizeof(struct image));
        if (open_image(bg, bg_file))
            goto nobg;

        term_vbe(bg);
        goto yesbg;

    nobg:
        term_vbe(NULL);

    yesbg:;
    }

    int timeout;
    if (!config_get_value(buf, 0, 16, "TIMEOUT")) {
        timeout = 5;
    } else {
        timeout = (int)strtoui(buf);
    }

    disable_cursor();
    int selected_entry = 0;
    bool skip_timeout = false;

refresh:
    clear(true);
    print("\n\n  \e[36m Limine " LIMINE_VERSION " \e[37m\n\n\n");

    print("Select an entry:\n\n");

    int max_entries;
    for (max_entries = 0; ; max_entries++) {
        if (config_get_entry_name(config_entry_name, max_entries, 1024) == -1)
            break;
        if (max_entries == selected_entry)
            print("  \e[47m\e[30m %s \e[40m\e[37m\n", config_entry_name);
        else
            print("   %s\n", config_entry_name);
    }

    if (max_entries == 0)
        panic("Config contains no entries.");

    print("\n");

    if (skip_timeout == false) {
        for (int i = timeout; i; i--) {
            print("\rBooting automatically in %u, press any key to stop the countdown...", i);
            if (pit_sleep_and_quit_on_keypress(18)) {
                skip_timeout = true;
                goto refresh;
            }
        }
        goto autoboot;
    }

    print("Arrows to choose, enter to select, 'e' to edit command line.");

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
            autoboot:
                config_set_entry(selected_entry);
                enable_cursor();
                if (!config_get_value(cmdline, 0, CMDLINE_MAX, "KERNEL_CMDLINE")) {
                    if (!config_get_value(cmdline, 0, CMDLINE_MAX, "CMDLINE")) {
                        cmdline[0] = '\0';
                    }
                }
                clear(true);
                return cmdline;
            case 'e':
                config_set_entry(selected_entry);
                enable_cursor();
                if (!config_get_value(cmdline, 0, CMDLINE_MAX, "KERNEL_CMDLINE")) {
                    if (!config_get_value(cmdline, 0, CMDLINE_MAX, "CMDLINE")) {
                        cmdline[0] = '\0';
                    }
                }
                print("\n\n> ");
                gets(cmdline, cmdline, CMDLINE_MAX);
                clear(true);
                return cmdline;
        }
    }
}
