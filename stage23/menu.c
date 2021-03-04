#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
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
    while (buffer[index] != '\n') {
        if (buffer[index] == 0)
            return index;
        index++;
    }
    index++;
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

static char *config_entry_editor(const char *orig_entry) {
    size_t cursor_offset  = 0;
    size_t entry_size     = strlen(orig_entry);
    size_t _window_size   = term_rows - 11;
    size_t window_offset  = 0;
    size_t line_size      = term_cols - 2;

    bool display_overflow_error = false;

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

    print("\n\xda");
    for (int i = 0; i < term_cols - 2; i++) {
        switch (i) {
            case 1: case 2: case 3:
                if (window_offset > 0) {
                    print("\x18");
                    break;
                }
                // FALLTHRU
            default:
                print("\xc4");
        }
    }
    print("\xbf\xb3");

    int cursor_x, cursor_y;
    size_t current_line = 0, line_offset = 0, window_size = _window_size;
    bool printed_cursor = false;
    for (size_t i = 0; ; i++) {
        if (buffer[i] == '\n'
         && current_line <  window_offset + window_size
         && current_line >= window_offset) {
            int x, y;
            get_cursor_pos(&x, &y);
            if (i == cursor_offset) {
                cursor_x = x;
                cursor_y = y;
                printed_cursor = true;
            }
            set_cursor_pos(term_cols - 1, y);
            if (current_line == window_offset + window_size - 1)
                print("\xb3\xc0");
            else
                print("\xb3\xb3");
            line_offset = 0;
            current_line++;
            continue;
        }

        if (line_offset && !(line_offset % line_size)) {
            window_size--;
            if (current_line == window_offset + window_size)
                print("\x1a\xc0");
            else
                print("\x1a\x1b\x1b");
        }

        if (i == cursor_offset
         && current_line <  window_offset + window_size
         && current_line >= window_offset) {
            get_cursor_pos(&cursor_x, &cursor_y);
            printed_cursor = true;
        }

        if (buffer[i] == 0 || current_line >= window_offset + window_size) {
            if (!printed_cursor) {
                if (i <= cursor_offset) {
                    window_offset++;
                    goto refresh;
                }
                if (i > cursor_offset) {
                    window_offset--;
                    goto refresh;
                }
            }
            break;
        }

        if (buffer[i] == '\n') {
            line_offset = 0;
            current_line++;
            continue;
        }

        if (current_line >= window_offset) {
            line_offset++;
            print("%c", buffer[i]);
        }
    }

    if (current_line - window_offset < window_size) {
        int x, y;
        for (size_t i = 0; i < (window_size - (current_line - window_offset)) - 1; i++) {
            get_cursor_pos(&x, &y);
            set_cursor_pos(term_cols - 1, y);
            print("\xb3\xb3");
        }
        get_cursor_pos(&x, &y);
        set_cursor_pos(term_cols - 1, y);
        print("\xb3\xc0");
    }

    for (int i = 0; i < term_cols - 2; i++) {
        switch (i) {
            case 1: case 2: case 3:
                if (current_line - window_offset >= window_size) {
                    print("\x19");
                    break;
                }
                // FALLTHRU
            default:
                print("\xc4");
        }
    }
    print("\xd9");

    if (display_overflow_error) {
        print(" ERR: Text buffer not big enough, delete something instead.");
        display_overflow_error = false;
    }

    // Hack to redraw the cursor
    set_cursor_pos(cursor_x, cursor_y);
    enable_cursor();

    term_double_buffer_flush();

    int c = getchar();
    switch (c) {
        case GETCHAR_CURSOR_DOWN:
            cursor_offset = get_next_line(cursor_offset, buffer);
            break;
        case GETCHAR_CURSOR_UP:
            cursor_offset = get_prev_line(cursor_offset, buffer);
            break;
        case GETCHAR_CURSOR_LEFT:
            if (cursor_offset) {
                cursor_offset--;
            }
            break;
        case GETCHAR_CURSOR_RIGHT:
            if (cursor_offset < strlen(buffer)) {
                cursor_offset++;
            }
            break;
        case GETCHAR_HOME: {
            size_t displacement;
            get_line_offset(&displacement, cursor_offset, buffer);
            cursor_offset -= displacement;
            break;
        }
        case GETCHAR_END: {
            cursor_offset += get_line_length(cursor_offset, buffer);
            break;
        }
        case '\b':
            if (cursor_offset) {
                cursor_offset--;
        case GETCHAR_DELETE:
                for (size_t i = cursor_offset; ; i++) {
                    buffer[i] = buffer[i+1];
                    if (!buffer[i])
                        break;
                }
            }
            break;
        case GETCHAR_F10:
            disable_cursor();
            return buffer;
        case '\e':
            disable_cursor();
            return NULL;
        default:
            if (strlen(buffer) < EDITOR_MAX_BUFFER_SIZE - 1) {
                for (size_t i = strlen(buffer); ; i--) {
                    buffer[i+1] = buffer[i];
                    if (i == cursor_offset)
                        break;
                }
                buffer[cursor_offset++] = c;
            } else {
                display_overflow_error = true;
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
            if ((c = pit_sleep_and_quit_on_keypress(1))) {
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
            case '\n':
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
                char *new_body = config_entry_editor(selected_menu_entry->body);
                if (new_body == NULL)
                    goto refresh;
                selected_menu_entry->body = new_body;
                goto autoboot;
            }
        }
    }
}
