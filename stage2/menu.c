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

static char *menu_branding = NULL;

static void cursor_back(void) {
    int x, y;
    get_cursor_pos(&x, &y);
    if (x) {
        x--;
    } else if (y) {
        y--;
        x = term_cols - 1;
    }
    set_cursor_pos(x, y);
}

static void cursor_fwd(void) {
    int x, y;
    get_cursor_pos(&x, &y);
    if (x < term_cols - 1) {
        x++;
    } else if (y < term_rows - 1) {
        y++;
        x = 0;
    }
    set_cursor_pos(x, y);
}

#define EDITOR_MAX_BUFFER_SIZE 4096

static size_t get_line_offset(size_t *displacement, size_t index, const char *buffer) {
    size_t offset = 0;
    size_t _index = index;
    for (size_t i = 0; buffer[i]; i++) {
        if (!_index--)
            break;
        if (buffer[i] == '\n')
            offset = i + 1;
    }
    if (displacement)
        *displacement = index - offset;
    return offset;
}

static size_t get_line_length(size_t index, const char *buffer) {
    size_t i;
    for (i = index; buffer[i] != '\n' && buffer[i] != 0; i++);
    return i - index;
}

static size_t get_next_line(size_t index, const char *buffer) {
    if (buffer[index] == 0)
        return index;
    size_t displacement;
    get_line_offset(&displacement, index, buffer);
    while (buffer[index++] != '\n');
    size_t next_line_length = get_line_length(index, buffer);
    if (displacement > next_line_length)
        displacement = next_line_length;
    return index + displacement;
}

static size_t get_prev_line(size_t index, const char *buffer) {
    size_t offset, displacement, prev_line_offset, prev_line_length;
    offset = get_line_offset(&displacement, index, buffer);
    if (offset) {
        prev_line_offset = get_line_offset(NULL, offset - 1, buffer);
        prev_line_length = get_line_length(prev_line_offset, buffer);
        if (displacement > prev_line_length)
            displacement = prev_line_length;
        return prev_line_offset + displacement;
    }
    return offset;
}

static char *config_entry_editor(bool *ret, const char *orig_entry) {
    size_t cursor_offset = 0;
    size_t entry_size = strlen(orig_entry);

    // Skip leading newlines
    while (*orig_entry == '\n') {
        orig_entry++;
        entry_size--;
    }

    if (entry_size >= EDITOR_MAX_BUFFER_SIZE)
        panic("Entry is too big to be edited.");

    char *buffer = ext_mem_alloc(EDITOR_MAX_BUFFER_SIZE);
    memcpy(buffer, orig_entry, entry_size);
    buffer[entry_size] = 0;

refresh:
    clear(true);
    disable_cursor();
    print("\n\n  \e[36m %s \e[37m\n\n\n", menu_branding);

    print("Editing entry.\n");
    print("Press esc to return to main menu and discard changes, press F10 to boot.\n");

    print("\n");
    for (int i = 0; i < term_cols; i++)
        print("-");

    int cursor_x, cursor_y;
    for (size_t i = 0; ; i++) {
        if (i == cursor_offset)
            get_cursor_pos(&cursor_x, &cursor_y);

        if (buffer[i] == 0)
            break;

        print("%c", buffer[i]);
    }

    print("\n");
    for (int i = 0; i < term_cols; i++)
        print("-");

    // Hack to redraw the cursor
    set_cursor_pos(cursor_x, cursor_y);
    enable_cursor();

    term_double_buffer_flush();

    int c = getchar();
    switch (c) {
        case 0:
        case GETCHAR_CURSOR_DOWN:
            cursor_offset = get_next_line(cursor_offset, buffer);
            break;
        case GETCHAR_CURSOR_UP:
            cursor_offset = get_prev_line(cursor_offset, buffer);
            break;
        case GETCHAR_CURSOR_LEFT:
            if (cursor_offset) {
                cursor_offset--;
                cursor_back();
            }
            break;
        case GETCHAR_CURSOR_RIGHT:
            if (cursor_offset < strlen(buffer)) {
                cursor_offset++;
                cursor_fwd();
            }
            break;
        case '\b':
            if (cursor_offset) {
                cursor_offset--;
                cursor_back();
        case GETCHAR_DELETE:
                for (size_t i = cursor_offset; ; i++) {
                    buffer[i] = buffer[i+1];
                    if (!buffer[i])
                        break;
                }
            }
            break;
        case GETCHAR_F10:
            *ret = false;
            disable_cursor();
            return buffer;
        case '\e':
            *ret = true;
            disable_cursor();
            return (char *)orig_entry;
        case '\r':
            c = '\n';
            // FALLTHRU
        default:
            if (strlen(buffer) < EDITOR_MAX_BUFFER_SIZE - 1) {
                for (size_t i = strlen(buffer); ; i--) {
                    buffer[i+1] = buffer[i];
                    if (i == cursor_offset)
                        break;
                }
                buffer[cursor_offset++] = c;
            }
            break;
    }

    goto refresh;
}

static int print_tree(int level, int base_index, int selected_entry,
                      struct menu_entry *current_entry,
                      struct menu_entry **selected_menu_entry) {
    int max_entries = 0;

    for (;;) {
        if (current_entry == NULL)
            break;
        if (level) {
            for (int i = level - 1; i > 0; i--) {
                struct menu_entry *actual_parent = current_entry;
                for (int j = 0; j < i; j++)
                    actual_parent = actual_parent->parent;
                if (actual_parent->next != NULL)
                    print(" \xb3");
                else
                    print("  ");
            }
            if (current_entry->next == NULL)
                print(" \xc0");
            else
                print(" \xc3");
        }
        if (current_entry->sub)
            print(current_entry->expanded ? "[-]" : "[+]");
        else if (level)
            print("\xc4> ");
        else
            print("   ");
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

char *menu(char **cmdline) {
    menu_branding = config_get_value(NULL, 0, "MENU_BRANDING");
    if (menu_branding == NULL)
        menu_branding = "Limine " LIMINE_VERSION;

    if (menu_tree == NULL)
        panic("Config contains no valid entries.");

    bool skip_timeout = false;
    struct menu_entry *selected_menu_entry = NULL;

    int selected_entry = 0;
    char *default_entry = config_get_value(NULL, 0, "DEFAULT_ENTRY");
    if (default_entry != NULL) {
        selected_entry = strtoui(default_entry, NULL, 10);
        if (selected_entry)
            selected_entry--;
    }

    int timeout = 5;
    char *timeout_config = config_get_value(NULL, 0, "TIMEOUT");
    if (timeout_config != NULL) {
        if (!strcmp(timeout_config, "no"))
            skip_timeout = true;
        else
            timeout = strtoui(timeout_config, NULL, 10);
    }

    if (!timeout) {
        // Use print tree to load up selected_menu_entry and determine if the
        // default entry is valid.
        print_tree(0, 0, selected_entry, menu_tree, &selected_menu_entry);
        if (selected_menu_entry == NULL || selected_menu_entry->sub != NULL) {
            print("Default entry is not valid or directory, booting to menu.\n");
            skip_timeout = true;
        } else {
            goto autoboot;
        }
    }

    // If there is GRAPHICS config key and the value is "yes", enable graphics
    char *graphics = config_get_value(NULL, 0, "GRAPHICS");
    if (graphics != NULL && !strcmp(graphics, "yes")) {
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

        char *colours = config_get_value(NULL, 0, "THEME_COLOURS");
        if (colours == NULL)
            colours = config_get_value(NULL, 0, "THEME_COLORS");
        if (colours != NULL) {
            const char *first = colours;
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

        char *theme_margin = config_get_value(NULL, 0, "THEME_MARGIN");
        if (theme_margin != NULL) {
            margin = strtoui(theme_margin, NULL, 10);
        }

        char *theme_margin_gradient = config_get_value(NULL, 0, "THEME_MARGIN_GRADIENT");
        if (theme_margin_gradient != NULL) {
            margin_gradient = strtoui(theme_margin_gradient, NULL, 10);
        }

        struct image *bg = NULL;

        char *background_path = config_get_value(NULL, 0, "BACKGROUND_PATH");
        if (background_path == NULL)
            goto nobg;

        struct file_handle *bg_file = ext_mem_alloc(sizeof(struct file_handle));
        if (!uri_open(bg_file, background_path))
            goto nobg;

        bg = ext_mem_alloc(sizeof(struct image));
        if (open_image(bg, bg_file))
            bg = NULL;

    nobg:
        term_vbe(colourscheme, margin, margin_gradient, bg);
    }

    disable_cursor();

    term_double_buffer(true);

refresh:
    clear(true);
    print("\n\n  \e[36m %s \e[37m\n\n\n", menu_branding);

    print("Select an entry:\n\n");

    int max_entries = print_tree(0, 0, selected_entry, menu_tree,
                                 &selected_menu_entry);

    print("\nArrows to choose, enter to boot, 'e' to edit selected entry.");

    if (selected_menu_entry->sub != NULL)
        skip_timeout = true;

    int c;

    if (skip_timeout == false) {
        print("\n\n");
        for (int i = timeout; i; i--) {
            print("\rBooting automatically in %u, press any key to stop the countdown...", i);
            term_double_buffer_flush();
            if ((c = pit_sleep_and_quit_on_keypress(18))) {
                skip_timeout = true;
                print("\e[2K\r\e[2A");
                goto timeout_aborted;
            }
        }
        goto autoboot;
    }

    term_double_buffer_flush();

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
                    selected_menu_entry->expanded = !selected_menu_entry->expanded;
                    goto refresh;
                }
                enable_cursor();
                *cmdline = config_get_value(selected_menu_entry->body, 0, "KERNEL_CMDLINE");
                if (!*cmdline) {
                    *cmdline = config_get_value(selected_menu_entry->body, 0, "CMDLINE");
                }
                if (!*cmdline) {
                    *cmdline = "";
                }
                clear(true);
                term_double_buffer(false);
                return selected_menu_entry->body;
            case 'e': {
                if (selected_menu_entry->sub != NULL)
                    goto refresh;
                enable_cursor();
                bool ret;
                selected_menu_entry->body = config_entry_editor(&ret, selected_menu_entry->body);
                if (ret)
                    goto refresh;
                goto autoboot;
            }
        }
    }
}
