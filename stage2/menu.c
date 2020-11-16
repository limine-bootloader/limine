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
#include <lib/readline.h>
#include <lib/uri.h>
#include <mm/pmm.h>
#include <drivers/vbe.h>

static char *cmdline;
#define CMDLINE_MAX 1024

static int print_tree(int level, int base_index, int selected_entry,
                         struct menu_entry *current_entry,
                         struct menu_entry **selected_menu_entry) {
    int max_entries = 0;
    for (;;) {
        if (current_entry == NULL)
            break;
        for (int i = 0; i < level; i++)
            print("  ");
        if (current_entry->sub)
            print(current_entry->expanded ? "[-] " : "[+] ");
        else
            print("    ");
        if (base_index + max_entries == selected_entry) {
            *selected_menu_entry = current_entry;
            print("\e[47m\e[30m");
        }
        print(" %s \e[0m\n", current_entry->name);
        if (current_entry->sub && current_entry->expanded) {
            max_entries += print_tree(level + 1, base_index + max_entries + 1,
                                      selected_entry,
                                      current_entry->sub,
                                      selected_menu_entry);
        }
        max_entries++;
        current_entry = current_entry->next;
    }
    return max_entries;
}

char *menu(char **cmdline_ret) {
    cmdline = conv_mem_alloc(CMDLINE_MAX);

    char *buf = conv_mem_alloc(256);

    struct menu_entry *selected_menu_entry;

    int selected_entry = 0;
    if (config_get_value(NULL, buf, 0, 16, "DEFAULT_ENTRY")) {
        selected_entry = (int)strtoui(buf, NULL, 10);
    }

    int timeout = 5;
    if (config_get_value(NULL, buf, 0, 16, "TIMEOUT")) {
        timeout = (int)strtoui(buf, NULL, 10);
    }

    if (!timeout)
        goto autoboot;

    // If there is GRAPHICS config key and the value is "yes", enable graphics
    if (config_get_value(NULL, buf, 0, 16, "GRAPHICS") && !strcmp(buf, "yes")) {
        // default scheme
        int margin = 64;
        int margin_gradient = 20;
        uint32_t colourscheme[] = {
            0x00000000, // black
            0x00aa0000, // red
            0x0000aa00, // green
            0x00aa5500, // brown
            0x000000aa, // blue
            0x00aa00aa, // magenta
            0x0000aaaa, // cyan
            0x00aaaaaa  // grey
        };

        if (config_get_value(NULL, buf, 0, 256, "THEME_COLOURS")
         || config_get_value(NULL, buf, 0, 256, "THEME_COLORS")) {
            const char *first = buf;
            for (int i = 0; i < 8; i++) {
                const char *last;
                uint32_t col = strtoui(first, &last, 16);
                if (first == last)
                    break;
                colourscheme[i] = col;
                if (*last == 0)
                    break;
                first = last + 1;
            }
        }

        if (config_get_value(NULL, buf, 0, 16, "THEME_MARGIN")) {
            margin = (int)strtoui(buf, NULL, 10);
        }

        if (config_get_value(NULL, buf, 0, 16, "THEME_MARGIN_GRADIENT")) {
            margin_gradient = (int)strtoui(buf, NULL, 10);
        }

        if (!config_get_value(NULL, cmdline, 0, CMDLINE_MAX, "BACKGROUND_PATH"))
            goto nobg;

        struct file_handle *bg_file = conv_mem_alloc(sizeof(struct file_handle));
        if (!uri_open(bg_file, cmdline))
            goto nobg;

        struct image *bg = conv_mem_alloc(sizeof(struct image));
        if (open_image(bg, bg_file))
            goto nobg;

        term_vbe(colourscheme, margin, margin_gradient, bg);
        goto yesbg;

    nobg:
        term_vbe(colourscheme, margin, margin_gradient, NULL);

    yesbg:;
    }

    disable_cursor();
    bool skip_timeout = false;

    if (menu_tree == NULL)
        panic("Config contains no entries.");

refresh:
    clear(true);
    print("\n\n  \e[36m Limine " LIMINE_VERSION " \e[37m\n\n\n");

    print("Select an entry:\n\n");

    int max_entries = print_tree(0, 0, selected_entry, menu_tree,
                                 &selected_menu_entry);

    print("\nArrows to choose, enter to select, 'e' to edit command line.");

    int c;

    if (skip_timeout == false) {
        print("\n\n");
        for (int i = timeout; i; i--) {
            print("\rBooting automatically in %u, press any key to stop the countdown...", i);
            if ((c = pit_sleep_and_quit_on_keypress(18))) {
                skip_timeout = true;
                print("\e[2K\r\e[2A");
                goto timeout_aborted;
            }
        }
        goto autoboot;
    }

    for (;;) {
        c = getchar();
timeout_aborted:
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
                if (selected_menu_entry->sub != NULL) {
                    skip_timeout = true;
                    selected_menu_entry->expanded = !selected_menu_entry->expanded;
                    goto refresh;
                }
                enable_cursor();
                if (!config_get_value(selected_menu_entry->body, cmdline, 0, CMDLINE_MAX, "KERNEL_CMDLINE")) {
                    if (!config_get_value(selected_menu_entry->body, cmdline, 0, CMDLINE_MAX, "CMDLINE")) {
                        cmdline[0] = '\0';
                    }
                }
                clear(true);
                *cmdline_ret = cmdline;
                return selected_menu_entry->body;
            case 'e':
                if (selected_menu_entry->sub != NULL)
                    goto refresh;
                enable_cursor();
                if (!config_get_value(selected_menu_entry->body, cmdline, 0, CMDLINE_MAX, "KERNEL_CMDLINE")) {
                    if (!config_get_value(selected_menu_entry->body, cmdline, 0, CMDLINE_MAX, "CMDLINE")) {
                        cmdline[0] = '\0';
                    }
                }
                print("\n\n> ");
                readline(cmdline, cmdline, CMDLINE_MAX);
                clear(true);
                *cmdline_ret = cmdline;
                return selected_menu_entry->body;
        }
    }
}
