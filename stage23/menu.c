#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <menu.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/config.h>
#include <lib/term.h>
#include <lib/gterm.h>
#include <lib/readline.h>
#include <lib/uri.h>
#include <mm/pmm.h>
#include <drivers/vbe.h>

static char *menu_branding = NULL;

#define EDITOR_MAX_BUFFER_SIZE 4096
#define TOK_KEY 0
#define TOK_EQUALS 1
#define TOK_VALUE 2
#define TOK_BADKEY 3
#define TOK_COMMENT 4

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

static const char *VALID_KEYS[] = {
    "TIMEOUT",
    "DEFAULT_ENTRY",
    "GRAPHICS",
    "MENU_RESOLUTION",
    "MENU_BRANDING",
    "MENU_FONT",
    "TERMINAL_FONT",
    "THEME_COLOURS",
    "THEME_COLORS",
    "THEME_BACKGROUND",
    "THEME_FOREGROUND",
    "THEME_MARGIN",
    "THEME_MARGIN_GRADIENT",
    "BACKGROUND_PATH",
    "BACKGROUND_STYLE",
    "BACKDROP_COLOUR",
    "BACKDROP_COLOR",
    "EDITOR_ENABLED",
    "EDITOR_HIGHLIGHTING",
    "EDITOR_VALIDATION",
    "VERBOSE",
    "PROTOCOL",
    "CMDLINE",
    "KERNEL_CMDLINE",
    "KERNEL_PATH",
    "MODULE_PATH",
    "MODULE_STRING",
    "RESOLUTION",
    "KASLR",
    "DRIVE",
    "PARTITION",
    "IMAGE_PATH",
    "COMMENT",
    NULL
};

static bool validation_enabled = true;
static bool invalid_syntax = false;

static int validate_line(const char *buffer) {
    if (!validation_enabled) return TOK_KEY;
    if (buffer[0] == '#')
        return TOK_COMMENT;
    char keybuf[64];
    int i;
    for (i = 0; buffer[i] && i < 64; i++) {
        if (buffer[i] == '=') goto found_equals;
        keybuf[i] = buffer[i];
    }
fail:
    if (i < 64) keybuf[i] = 0;
    if (keybuf[0] == '\n' || (!keybuf[0] && buffer[0] != '=')) return TOK_KEY; // blank line is valid
    invalid_syntax = true;
    return TOK_BADKEY;
found_equals:
    if (i < 64) keybuf[i] = 0;
    for (i = 0; VALID_KEYS[i]; i++) {
        if (!strcmp(keybuf, VALID_KEYS[i])) {
            return TOK_KEY;
        }
    }
    goto fail;
}

static char *config_entry_editor(const char *title, const char *orig_entry) {
    size_t cursor_offset  = 0;
    size_t entry_size     = strlen(orig_entry);
    size_t _window_size   = term_rows - 8;
    size_t window_offset  = 0;
    size_t line_size      = term_cols - 2;

    bool display_overflow_error = false;

    // Skip leading newlines
    while (*orig_entry == '\n') {
        orig_entry++;
        entry_size--;
    }

    if (entry_size >= EDITOR_MAX_BUFFER_SIZE) {
        panic("Entry is too big to be edited.");
    }

    bool syntax_highlighting_enabled = true;
    char *syntax_highlighting_enabled_config = config_get_value(NULL, 0, "EDITOR_HIGHLIGHTING");
    if (!strcmp(syntax_highlighting_enabled_config, "no")) syntax_highlighting_enabled = false;

    validation_enabled = true;
    char *validation_enabled_config = config_get_value(NULL, 0, "EDITOR_VALIDATION");
    if (!strcmp(validation_enabled_config, "no")) validation_enabled = false;

    static char *buffer = NULL;
    if (buffer == NULL)
        buffer = ext_mem_alloc(EDITOR_MAX_BUFFER_SIZE);
    memcpy(buffer, orig_entry, entry_size);
    buffer[entry_size] = 0;

refresh:
    invalid_syntax = false;

    clear(true);
    disable_cursor();
    {
        int x, y;
        print("\n");
        get_cursor_pos(&x, &y);
        set_cursor_pos(term_cols / 2 - DIV_ROUNDUP(strlen(menu_branding), 2), y);
        print("\e[36m%s\e[37m", menu_branding);
        print("\n\n");
    }

    print("    \e[32mESC\e[0m Discard and Exit    \e[32mF10\e[0m Boot\n");

    print("\n\xda");
    for (int i = 0; i < term_cols - 2; i++) {
        switch (i) {
            case 1: case 2: case 3:
                if (window_offset > 0) {
                    print("\x18");
                    break;
                }
                // FALLTHRU
            default: {
                int title_length = strlen(title);
                if (i == (term_cols / 2) - DIV_ROUNDUP(title_length, 2) - 1) {
                    print("%s", title);
                    i += title_length - 1;
                } else {
                    print("\xc4");
                }
            }
        }
    }
    print("\xbf\xb3");

    int cursor_x, cursor_y;
    size_t current_line = 0, line_offset = 0, window_size = _window_size;
    bool printed_cursor = false;
    int token_type = validate_line(buffer);
    for (size_t i = 0; ; i++) {
        // newline
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
            token_type = validate_line(buffer+i+1);
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

            // switch to token type 1 if equals sign
            if (token_type == TOK_KEY && buffer[i] == '=') token_type = TOK_EQUALS;

            // syntax highlighting
            if (syntax_highlighting_enabled) {
                switch (token_type) {
                    case TOK_KEY:
                        print("\e[36m%c\e[0m", buffer[i]);
                        break;
                    case TOK_EQUALS:
                        print("\e[32m%c\e[0m", buffer[i]);
                        break;
                    case TOK_VALUE:
                        print("\e[39m%c\e[0m", buffer[i]);
                        break;
                    case TOK_BADKEY:
                        print("\e[31m%c\e[0m", buffer[i]);
                        break;
                    case TOK_COMMENT:
                        print("\e[33m%c\e[0m", buffer[i]);
               }
           } else {
               print("%c", buffer[i]);
           }

           // switch to token type 2 after equals sign
           if (token_type == TOK_EQUALS) token_type = TOK_VALUE;
        }
    }

    // syntax error alert
    if (validation_enabled) {
        int x, y;
        get_cursor_pos(&x, &y);
        set_cursor_pos(0, term_rows-1);
        scroll_disable();
        if (invalid_syntax) {
            print("\e[31mConfiguration is INVALID.\e[0m");
        } else {
            print("\e[32mConfiguration is valid.\e[0m");
        }
        scroll_enable();
        set_cursor_pos(x, y);
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
        scroll_disable();
        print("\e[31mText buffer not big enough, delete something instead.");
        scroll_enable();
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
        case GETCHAR_ESCAPE:
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

static int print_tree(const char *shift, int level, int base_index, int selected_entry,
                      struct menu_entry *current_entry,
                      struct menu_entry **selected_menu_entry) {
    int max_entries = 0;

    bool no_print = false;
    if (shift == NULL) {
        no_print = true;
    }

    for (;;) {
        if (current_entry == NULL)
            break;
        if (!no_print) print("%s", shift);
        if (level) {
            for (int i = level - 1; i > 0; i--) {
                struct menu_entry *actual_parent = current_entry;
                for (int j = 0; j < i; j++)
                    actual_parent = actual_parent->parent;
                if (actual_parent->next != NULL) {
                    if (!no_print) print(" \xb3");
                } else {
                    if (!no_print) print("  ");
                }
            }
            if (current_entry->next == NULL) {
                if (!no_print) print(" \xc0");
            } else {
                if (!no_print) print(" \xc3");
            }
        }
        if (current_entry->sub) {
            if (!no_print) print(current_entry->expanded ? "[-]" : "[+]");
        } else if (level) {
            if (!no_print) print("\xc4> ");
        } else {
            if (!no_print) print("   ");
        }
        if (base_index + max_entries == selected_entry) {
            *selected_menu_entry = current_entry;
            if (!no_print) print("\e[47m\e[30m");
        }
        if (!no_print) print(" %s \e[0m\n", current_entry->name);
        if (current_entry->sub && current_entry->expanded) {
            max_entries += print_tree(shift, level + 1, base_index + max_entries + 1,
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

    bool editor_enabled = true;
    char *editor_enabled_config = config_get_value(NULL, 0, "EDITOR_ENABLED");
    if (!strcmp(editor_enabled_config, "no")) editor_enabled = false;

    if (!timeout) {
        // Use print tree to load up selected_menu_entry and determine if the
        // default entry is valid.
        print_tree(NULL, 0, 0, selected_entry, menu_tree, &selected_menu_entry);
        if (selected_menu_entry == NULL || selected_menu_entry->sub != NULL) {
            print("Default entry is not valid or directory, booting to menu.\n");
            skip_timeout = true;
        } else {
            goto autoboot;
        }
    }

    // If there is GRAPHICS config key and the value is "yes", enable graphics
#if bios == 1
    char *graphics = config_get_value(NULL, 0, "GRAPHICS");
#elif uefi == 1
    char *graphics = "yes";
#endif
    if (graphics != NULL && !strcmp(graphics, "yes")) {
        int req_width = 0, req_height = 0, req_bpp = 0;

        char *menu_resolution = config_get_value(NULL, 0, "MENU_RESOLUTION");
        if (menu_resolution != NULL)
            parse_resolution(&req_width, &req_height, &req_bpp, menu_resolution);

        term_vbe(req_width, req_height);
    }

    disable_cursor();

    term_double_buffer(true);

refresh:
    clear(true);
    {
        int x, y;
        print("\n");
        get_cursor_pos(&x, &y);
        set_cursor_pos(term_cols / 2 - DIV_ROUNDUP(strlen(menu_branding), 2), y);
        print("\e[36m%s\e[37m", menu_branding);
        print("\n\n\n\n");
    }

    if (menu_tree == NULL) {
        print("Config file %s.\n\n", config_ready ? "contains no valid entries" : "not found");
        print("For information on the format of Limine config entries, consult CONFIG.md in\n");
        print("the root of the Limine source repository.\n\n");
        print("Press a key to enter an editor session and manually define a config entry...");
        term_double_buffer_flush();
        getchar();
        char *new_body = NULL;
        while (new_body == NULL)
            new_body = config_entry_editor("New Entry", "");
        selected_menu_entry = ext_mem_alloc(sizeof(struct menu_entry));
        selected_menu_entry->body = new_body;
        config_ready = true;
        goto autoboot;
    }

    {   // Draw box around boot menu
        int x, y;
        get_cursor_pos(&x, &y);

        print("\xda");
        for (int i = 0; i < term_cols - 2; i++) {
            print("\xc4");
        }
        print("\xbf");

        for (int i = y + 1; i < term_rows - 2; i++) {
            set_cursor_pos(0, i);
            print("\xb3");
            set_cursor_pos(term_cols - 1, i);
            print("\xb3");
        }

        print("\xc0");
        for (int i = 0; i < term_cols - 2; i++) {
            print("\xc4");
        }
        print("\xd9");

        set_cursor_pos(x, y + 2);
    }

    int max_entries = print_tree("\xb3   ", 0, 0, selected_entry, menu_tree,
                                 &selected_menu_entry);

    {
        int x, y;
        get_cursor_pos(&x, &y);
        set_cursor_pos(0, 3);
        if (editor_enabled && selected_menu_entry->sub == NULL) {
            print("    \e[32mARROWS\e[0m Select    \e[32mENTER\e[0m Boot    \e[32mE\e[0m Edit");
        } else {
            print("    \e[32mARROWS\e[0m Select    \e[32mENTER\e[0m %s",
                  selected_menu_entry->expanded ? "Collapse" : "Expand");
        }
        set_cursor_pos(x, y);
    }

    if (selected_menu_entry->sub != NULL)
        skip_timeout = true;

    int c;

    if (skip_timeout == false) {
        print("\n\n");
        for (int i = timeout; i; i--) {
            set_cursor_pos(0, term_rows - 1);
            scroll_disable();
            print("\e[32mBooting automatically in %u, press any key to stop the countdown...\e[0m", i);
            scroll_enable();
            term_double_buffer_flush();
            if ((c = pit_sleep_and_quit_on_keypress(1))) {
                skip_timeout = true;
                print("\e[2K");
                term_double_buffer_flush();
                goto timeout_aborted;
            }
        }
        goto autoboot;
    }

    set_cursor_pos(0, term_rows - 1);
    if (selected_menu_entry->comment != NULL) {
        scroll_disable();
        print("\e[36m%s\e[0m", selected_menu_entry->comment);
        scroll_enable();
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
            case GETCHAR_CURSOR_RIGHT:
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
                if (editor_enabled) {
                    if (selected_menu_entry->sub != NULL)
                        goto refresh;
                    enable_cursor();
                    char *new_body = config_entry_editor(selected_menu_entry->name, selected_menu_entry->body);
                    if (new_body == NULL)
                        goto refresh;
                    selected_menu_entry->body = new_body;
                    goto autoboot;
                }
            }
        }
    }
}
