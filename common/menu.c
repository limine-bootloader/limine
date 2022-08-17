#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdnoreturn.h>
#include <config.h>
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
#include <console.h>
#include <protos/linux.h>
#include <protos/chainload.h>
#include <protos/chainload_next.h>
#include <protos/multiboot1.h>
#include <protos/multiboot2.h>
#include <protos/limine.h>

static char *menu_branding = NULL;
static char *menu_branding_colour = NULL;

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
    "COMMENT",
    "PROTOCOL",
    "CMDLINE",
    "KERNEL_CMDLINE",
    "KERNEL_PATH",
    "INITRD_PATH",
    "MODULE_PATH",
    "MODULE_STRING",
    "MODULE_CMDLINE",
    "RESOLUTION",
    "TEXTMODE",
    "KASLR",
    "DRIVE",
    "PARTITION",
    "IMAGE_PATH",
    "TERM_CONFIG_OVERRIDE",
    NULL
};

static bool validation_enabled = true;
static bool invalid_syntax = false;

static int validate_line(const char *buffer) {
    if (!validation_enabled) return TOK_KEY;
    if (buffer[0] == '#')
        return TOK_COMMENT;
    char keybuf[64];
    size_t i;
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

static void putchar_tokencol(int type, char c) {
    switch (type) {
        case TOK_KEY:
            print("\e[36m%c\e[0m", c);
            break;
        case TOK_EQUALS:
            print("\e[32m%c\e[0m", c);
            break;
        default:
        case TOK_VALUE:
            print("\e[39m%c\e[0m", c);
            break;
        case TOK_BADKEY:
            print("\e[31m%c\e[0m", c);
            break;
        case TOK_COMMENT:
            print("\e[33m%c\e[0m", c);
            break;
    }
}

static bool editor_no_term_reset = false;

char *config_entry_editor(const char *title, const char *orig_entry) {
    term_autoflush = false;

    enable_cursor();

    print("\e[2J\e[H");

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
        panic(true, "Entry is too big to be edited.");
    }

    bool syntax_highlighting_enabled = true;
    char *syntax_highlighting_enabled_config = config_get_value(NULL, 0, "EDITOR_HIGHLIGHTING");
    if (syntax_highlighting_enabled_config != NULL
     && strcmp(syntax_highlighting_enabled_config, "no") == 0) {
        syntax_highlighting_enabled = false;
    }

    validation_enabled = true;
    char *validation_enabled_config = config_get_value(NULL, 0, "EDITOR_VALIDATION");
    if (validation_enabled_config != NULL
     && strcmp(validation_enabled_config, "no") == 0) {
        validation_enabled = false;
    }

    char *buffer = ext_mem_alloc(EDITOR_MAX_BUFFER_SIZE);
    memcpy(buffer, orig_entry, entry_size);
    buffer[entry_size] = 0;

refresh:
    invalid_syntax = false;

    print("\e[2J\e[H");
    disable_cursor();
    {
        size_t x, y;
        print("\n");
        get_cursor_pos(&x, &y);
        set_cursor_pos_helper(term_cols / 2 - DIV_ROUNDUP(strlen(menu_branding), 2), y);
        print("\e[3%sm%s\e[37m", menu_branding_colour, menu_branding);
        print("\n\n");
    }

    print("    \e[32mESC\e[0m Discard and Exit    \e[32mF10\e[0m Boot\n\n");

    print(serial ? "/" : "\xda");
    for (size_t i = 0; i < term_cols - 2; i++) {
        switch (i) {
            case 1: case 2: case 3:
                if (window_offset > 0) {
                    print(serial ? "^" : "\x18");
                    break;
                }
                // FALLTHRU
            default: {
                size_t title_length = strlen(title);
                if (i == (term_cols / 2) - DIV_ROUNDUP(title_length, 2) - 1) {
                    print("%s", title);
                    i += title_length - 1;
                } else {
                    print(serial ? "-" : "\xc4");
                }
            }
        }
    }
    size_t tmpx, tmpy;

    get_cursor_pos(&tmpx, &tmpy);
    print(serial ? "\\" : "\xbf");
    set_cursor_pos_helper(0, tmpy + 1);
    print(serial ? "|" : "\xb3");

    size_t cursor_x, cursor_y;
    size_t current_line = 0, line_offset = 0, window_size = _window_size;
    bool printed_cursor = false;
    bool printed_early = false;
    int token_type = validate_line(buffer);
    for (size_t i = 0; ; i++) {
        // newline
        if (buffer[i] == '\n'
         && current_line <  window_offset + window_size
         && current_line >= window_offset) {
            size_t x, y;
            get_cursor_pos(&x, &y);
            if (i == cursor_offset) {
                cursor_x = x;
                cursor_y = y;
                printed_cursor = true;
            }
            set_cursor_pos_helper(term_cols - 1, y);
            if (current_line == window_offset + window_size - 1) {
                get_cursor_pos(&tmpx, &tmpy);
                print(serial ? "|" : "\xb3");
                set_cursor_pos_helper(0, tmpy + 1);
                print(serial ? "\\" : "\xc0");
            } else {
                get_cursor_pos(&tmpx, &tmpy);
                print(serial ? "|" : "\xb3");
                set_cursor_pos_helper(0, tmpy + 1);
                print(serial ? "|" : "\xb3");
            }
            line_offset = 0;
            token_type = validate_line(buffer + i + 1);
            current_line++;
            continue;
        }

        // switch to token type 1 if equals sign
        if (token_type == TOK_KEY && buffer[i] == '=') token_type = TOK_EQUALS;

        if (buffer[i] != 0 && line_offset % line_size == line_size - 1) {
            if (current_line <  window_offset + window_size
             && current_line >= window_offset) {
                if (i == cursor_offset) {
                    get_cursor_pos(&cursor_x, &cursor_y);
                    printed_cursor = true;
                }
                if (syntax_highlighting_enabled) {
                    putchar_tokencol(token_type, buffer[i]);
                } else {
                    print("%c", buffer[i]);
                }
                printed_early = true;
                size_t x, y;
                get_cursor_pos(&x, &y);
                if (y == term_rows - 3) {
                    print(serial ? ">" : "\x1a");
                    set_cursor_pos_helper(0, y + 1);
                    print(serial ? "\\" : "\xc0");
                } else {
                    print(serial ? ">" : "\x1a");
                    set_cursor_pos_helper(0, y + 1);
                    print(serial ? "<" : "\x1b\x1b");
                }
            }
            window_size--;
        }

        if (i == cursor_offset
         && current_line <  window_offset + window_size
         && current_line >= window_offset
         && !printed_cursor) {
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
            token_type = validate_line(buffer + i + 1);
            current_line++;
            continue;
        }

        if (current_line >= window_offset) {
            line_offset++;

            // syntax highlighting
            if (!printed_early) {
                if (syntax_highlighting_enabled) {
                    putchar_tokencol(token_type, buffer[i]);
                } else {
                    print("%c", buffer[i]);
                }
            }

            printed_early = false;

            // switch to token type 2 after equals sign
            if (token_type == TOK_EQUALS) token_type = TOK_VALUE;
        }
    }

    // syntax error alert
    if (validation_enabled) {
        size_t x, y;
        get_cursor_pos(&x, &y);
        set_cursor_pos_helper(0, term_rows-1);
        scroll_disable();
        if (invalid_syntax) {
            print("\e[31mConfiguration is INVALID.\e[0m");
        } else {
            print("\e[32mConfiguration is valid.\e[0m");
        }
        scroll_enable();
        set_cursor_pos_helper(x, y);
    }

    if (current_line - window_offset < window_size) {
        size_t x, y;
        for (size_t i = 0; i < (window_size - (current_line - window_offset)) - 1; i++) {
            get_cursor_pos(&x, &y);
            set_cursor_pos_helper(term_cols - 1, y);
            print(serial ? "|" : "\xb3");
            set_cursor_pos_helper(0, y + 1);
            print(serial ? "|" : "\xb3");
        }
        get_cursor_pos(&x, &y);
        set_cursor_pos_helper(term_cols - 1, y);
        print(serial ? "|" : "\xb3");
        set_cursor_pos_helper(0, y + 1);
        print(serial ? "\\" : "\xc0");
    }

    for (size_t i = 0; i < term_cols - 2; i++) {
        switch (i) {
            case 1: case 2: case 3:
                if (current_line - window_offset >= window_size) {
                    print(serial ? "v" : "\x19");
                    break;
                }
                // FALLTHRU
            default:
                print(serial ? "-" : "\xc4");
        }
    }
    get_cursor_pos(&tmpx, &tmpy);
    print(serial ? "/" : "\xd9");
    set_cursor_pos_helper(0, tmpy + 1);

    if (display_overflow_error) {
        scroll_disable();
        print("\e[31mText buffer not big enough, delete something instead.");
        scroll_enable();
        display_overflow_error = false;
    }

    // Hack to redraw the cursor
    set_cursor_pos_helper(cursor_x, cursor_y);
    enable_cursor();

    term_double_buffer_flush();

    int c = getchar();
    size_t buffer_len = strlen(buffer);
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
            if (cursor_offset < buffer_len) {
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
            editor_no_term_reset ? editor_no_term_reset = false : reset_term();
            return buffer;
        case GETCHAR_ESCAPE:
            pmm_free(buffer, EDITOR_MAX_BUFFER_SIZE);
            editor_no_term_reset ? editor_no_term_reset = false : reset_term();
            return NULL;
        default:
            if (buffer_len < EDITOR_MAX_BUFFER_SIZE - 1) {
                if (isprint(c) || c == '\n') {
                    for (size_t i = buffer_len; ; i--) {
                        buffer[i+1] = buffer[i];
                        if (i == cursor_offset)
                            break;
                    }
                    buffer[cursor_offset++] = c;
                }
            } else {
                display_overflow_error = true;
            }
            break;
    }

    goto refresh;
}

static size_t print_tree(const char *shift, size_t level, size_t base_index, size_t selected_entry,
                      struct menu_entry *current_entry,
                      struct menu_entry **selected_menu_entry) {
    size_t max_entries = 0;

    bool no_print = false;
    if (shift == NULL) {
        no_print = true;
    }

    for (;;) {
        if (current_entry == NULL)
            break;
        if (!no_print) print("%s", shift);
        if (level) {
            for (size_t i = level - 1; i > 0; i--) {
                struct menu_entry *actual_parent = current_entry;
                for (size_t j = 0; j < i; j++)
                    actual_parent = actual_parent->parent;
                if (actual_parent->next != NULL) {
                    if (!no_print) print(serial ? " |" : " \xb3");
                } else {
                    if (!no_print) print("  ");
                }
            }
            if (current_entry->next == NULL) {
                if (!no_print) print(serial ? " `" : " \xc0");
            } else {
                if (!no_print) print(serial ? " |" : " \xc3");
            }
        }
        if (current_entry->sub) {
            if (!no_print) print(current_entry->expanded ? "[-]" : "[+]");
        } else if (level) {
            if (!no_print) print(serial ? "-> " : "\xc4> ");
        } else {
            if (!no_print) print("   ");
        }
        if (base_index + max_entries == selected_entry) {
            *selected_menu_entry = current_entry;
            if (!no_print) print("\e[7m");
        }
        if (!no_print) print(" %s \e[27m\n", current_entry->name);
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

#if defined (__x86_64__) || defined (__aarch64__)
__attribute__((used))
static uintptr_t stack_at_first_entry = 0;
#endif

__attribute__((naked))
noreturn void menu(__attribute__((unused)) bool timeout_enabled) {
#if defined (__i386__)
    asm volatile (
        "pop %eax\n\t"
        "call 1f\n\t"
        "1:\n\t"
        "pop %eax\n\t"
        "add $(2f - 1b), %eax\n\t"
        "cmpl $0, (%eax)\n\t"
        "jne 1f\n\t"
        "mov %esp, (%eax)\n\t"
        "jmp 3f\n\t"
        "1:\n\t"
        "mov (%esp), %edi\n\t"
        "mov (%eax), %esp\n\t"
        "push %edi\n\t"
        "jmp 3f\n\t"
        "2:\n\t"
        ".long 0\n\t"
        "3:\n\t"
        "push $0\n\t"
        "jmp _menu"
    );
#elif defined (__x86_64__)
    asm volatile (
        "xor %eax, %eax\n\t"
        "cmp %rax, stack_at_first_entry(%rip)\n\t"
        "jne 1f\n\t"
        "mov %rsp, stack_at_first_entry(%rip)\n\t"
        "jmp 2f\n\t"
        "1:\n\t"
        "mov stack_at_first_entry(%rip), %rsp\n\t"
        "2:\n\t"
        "push $0\n\t"
        "push $0\n\t"
        "jmp _menu"
    );
#elif defined (__aarch64__)
    asm volatile (
        "adrp x8, stack_at_first_entry\n\t"
        "ldr x9, [x8, :lo12:stack_at_first_entry]\n\t"
        "cbz x9, 1f\n\t"
        "mov sp, x9\n\t"
        "b 2f\n\t"
        "1:\n\t"
        "mov x9, sp\n\t"
        "str x9, [x8, :lo12:stack_at_first_entry]\n\t"
        "2:\n\t"
        "mov x30, xzr\n\t"
        "mov x29, xzr\n\t"
        "b _menu"
    );

#else
#error Unknown architecture
#endif
}

static struct memmap_entry *rewound_memmap = NULL;
static size_t rewound_memmap_entries = 0;
static uint8_t *rewound_data;
#if bios == 1
static uint8_t *rewound_s2_data;
#endif

extern symbol data_begin;
extern symbol data_end;
#if bios == 1
extern symbol s2_data_begin;
extern symbol s2_data_end;
#endif

__attribute__((used))
static noreturn void _menu(bool timeout_enabled) {
    size_t data_size = (uintptr_t)data_end - (uintptr_t)data_begin;
#if bios == 1
    size_t s2_data_size = (uintptr_t)s2_data_end - (uintptr_t)s2_data_begin;
#endif

    if (rewound_memmap != NULL) {
        memcpy(data_begin, rewound_data, data_size);
#if bios == 1
        memcpy(s2_data_begin, rewound_s2_data, s2_data_size);
#endif
        memcpy(memmap, rewound_memmap, rewound_memmap_entries * sizeof(struct memmap_entry));
        memmap_entries = rewound_memmap_entries;
    } else {
        rewound_data = ext_mem_alloc(data_size);
#if bios == 1
        rewound_s2_data = ext_mem_alloc(s2_data_size);
#endif
        rewound_memmap = ext_mem_alloc(256 * sizeof(struct memmap_entry));
        memcpy(rewound_memmap, memmap, memmap_entries * sizeof(struct memmap_entry));
        rewound_memmap_entries = memmap_entries;
        memcpy(rewound_data, data_begin, data_size);
#if bios == 1
        memcpy(rewound_s2_data, s2_data_begin, s2_data_size);
#endif
    }

    if (bad_config == false) {
        volume_iterate_parts(boot_volume,
            if (!init_config_disk(_PART)) {
                boot_volume = _PART;
                break;
            }
        );
    }

    char *quiet_str = config_get_value(NULL, 0, "QUIET");
    quiet = quiet_str != NULL && strcmp(quiet_str, "yes") == 0;

    char *verbose_str = config_get_value(NULL, 0, "VERBOSE");
    verbose = verbose_str != NULL && strcmp(verbose_str, "yes") == 0;

    char *serial_str = config_get_value(NULL, 0, "SERIAL");
    serial = serial_str != NULL && strcmp(serial_str, "yes") == 0;

    char *randomise_mem_str = config_get_value(NULL, 0, "RANDOMISE_MEMORY");
    if (randomise_mem_str == NULL)
        randomise_mem_str = config_get_value(NULL, 0, "RANDOMIZE_MEMORY");
    bool randomise_mem = randomise_mem_str != NULL && strcmp(randomise_mem_str, "yes") == 0;
    if (randomise_mem) {
        term_vbe(NULL, 0, 0);
        early_term = true;
        pmm_randomise_memory();
    }

    char *editor_enabled_str = config_get_value(NULL, 0, "EDITOR_ENABLED");
    if (editor_enabled_str != NULL) {
        editor_enabled = strcmp(editor_enabled_str, "yes") == 0;
    }

    menu_branding = config_get_value(NULL, 0, "INTERFACE_BRANDING");
    if (menu_branding == NULL)
        menu_branding = "Limine " LIMINE_VERSION;

    menu_branding_colour = config_get_value(NULL, 0, "INTERFACE_BRANDING_COLOUR");
    if (menu_branding_colour == NULL)
        menu_branding_colour = config_get_value(NULL, 0, "INTERFACE_BRANDING_COLOR");
    if (menu_branding_colour == NULL)
        menu_branding_colour = "6";

    bool skip_timeout = false;
    struct menu_entry *selected_menu_entry = NULL;

    size_t selected_entry = 0;
    char *default_entry = config_get_value(NULL, 0, "DEFAULT_ENTRY");
    if (default_entry != NULL) {
        selected_entry = strtoui(default_entry, NULL, 10);
        if (selected_entry)
            selected_entry--;
    }

    size_t timeout = 5;
    char *timeout_config = config_get_value(NULL, 0, "TIMEOUT");
    if (timeout_config != NULL) {
        if (!strcmp(timeout_config, "no"))
            skip_timeout = true;
        else
            timeout = strtoui(timeout_config, NULL, 10);
    }

    if (!timeout_enabled) {
        skip_timeout = true;
    }

    if (!skip_timeout && !timeout) {
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

reterm:
    if (graphics == NULL || strcmp(graphics, "no") != 0) {
        size_t req_width = 0, req_height = 0, req_bpp = 0;

        char *menu_resolution = config_get_value(NULL, 0, "INTERFACE_RESOLUTION");
        if (menu_resolution != NULL)
            parse_resolution(&req_width, &req_height, &req_bpp, menu_resolution);

        term_vbe(NULL, req_width, req_height);
    } else {
#if bios == 1
        term_textmode();
#endif
    }

refresh:
    term_autoflush = false;

    disable_cursor();

    print("\e[2J\e[H");
    {
        size_t x, y;
        print("\n");
        get_cursor_pos(&x, &y);
        set_cursor_pos_helper(term_cols / 2 - DIV_ROUNDUP(strlen(menu_branding), 2), y);
        print("\e[3%sm%s\e[37m", menu_branding_colour, menu_branding);
        print("\n\n\n\n");
    }

    while (menu_tree == NULL) {
        print("Config file %s.\n\n", config_ready ? "contains no valid entries" : "not found");
        print("For information on the format of Limine config entries, consult CONFIG.md in\n");
        print("the root of the Limine source repository.\n\n");
        print("Press a key to enter the Limine console...");
        term_double_buffer_flush();
        getchar();
        reset_term();
        console();
    }

    {   // Draw box around boot menu
        size_t x, y;
        get_cursor_pos(&x, &y);

        print(serial ? "/" : "\xda");
        for (size_t i = 0; i < term_cols - 2; i++) {
            print(serial ? "-" : "\xc4");
        }
        print(serial ? "\\" : "\xbf");

        for (size_t i = y + 1; i < term_rows - 2; i++) {
            set_cursor_pos_helper(0, i);
            print(serial ? "|" : "\xb3");
            set_cursor_pos_helper(term_cols - 1, i);
            print(serial ? "|" : "\xb3");
        }
        set_cursor_pos_helper(0, term_rows - 2);

        print(serial ? "\\" : "\xc0");
        for (size_t i = 0; i < term_cols - 2; i++) {
            print(serial ? "-" : "\xc4");
        }
        print(serial ? "/" : "\xd9");

        set_cursor_pos_helper(x, y + 2);
    }

    size_t max_entries = print_tree(serial ? "|   " : "\xb3   ", 0, 0, selected_entry, menu_tree,
                                 &selected_menu_entry);

    {
        size_t x, y;
        get_cursor_pos(&x, &y);
        set_cursor_pos_helper(0, 3);
        if (editor_enabled && selected_menu_entry->sub == NULL) {
            print("    \e[32mARROWS\e[0m Select    \e[32mENTER\e[0m Boot    \e[32mE\e[0m Edit");
        } else {
            print("    \e[32mARROWS\e[0m Select    \e[32mENTER\e[0m %s",
                  selected_menu_entry->expanded ? "Collapse" : "Expand");
        }
        set_cursor_pos_helper(term_cols - 13, 3);
        print("\e[32mC\e[0m Console");
        set_cursor_pos_helper(x, y);
    }

    if (selected_menu_entry->sub != NULL)
        skip_timeout = true;

    int c;

    if (skip_timeout == false) {
        print("\n\n");
        for (size_t i = timeout; i; i--) {
            set_cursor_pos_helper(0, term_rows - 1);
            scroll_disable();
            print("\e[2K\e[32mBooting automatically in \e[92m%u\e[32m, press any key to stop the countdown...\e[0m", i);
            scroll_enable();
            term_double_buffer_flush();
            if ((c = pit_sleep_and_quit_on_keypress(1))) {
                skip_timeout = true;
                if (quiet) {
                    quiet = false;
                    goto reterm;
                } else {
                    print("\e[2K");
                    term_double_buffer_flush();
                    goto timeout_aborted;
                }
            }
        }
        goto autoboot;
    }

    set_cursor_pos_helper(0, term_rows - 1);
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
            case '1': case '2': case '3': case '4': case '5':
            case '6': case '7': case '8': case '9': {
                int ent = (c - '0') - 1;
                if (ent < (int)max_entries) {
                    selected_entry = ent;
                    print_tree(NULL, 0, 0, selected_entry, menu_tree,
                               &selected_menu_entry);
                    goto autoboot;
                }
                goto refresh;
            }
            case GETCHAR_HOME:
                selected_entry = 0;
                goto refresh;
            case GETCHAR_END:
                selected_entry = max_entries - 1;
                goto refresh;
            case GETCHAR_CURSOR_UP:
                if (selected_entry == 0)
                    selected_entry = max_entries - 1;
                else
                    selected_entry--;
                goto refresh;
            case GETCHAR_CURSOR_DOWN:
                if (++selected_entry == max_entries)
                    selected_entry = 0;
                goto refresh;
            case GETCHAR_CURSOR_RIGHT:
            case '\n':
            case ' ':
            autoboot:
                if (selected_menu_entry->sub != NULL) {
                    selected_menu_entry->expanded = !selected_menu_entry->expanded;
                    goto refresh;
                }
                if (term_backend == NOT_READY) {
                    term_vbe(NULL, 0, 0);
#if bios == 1
                    if (term_backend == NOT_READY) {
                        term_textmode();
                    }
#endif
                } else {
                    reset_term();
                }
                boot(selected_menu_entry->body);
            case 'e':
            case 'E': {
                if (editor_enabled) {
                    if (selected_menu_entry->sub != NULL)
                        goto refresh;
                    editor_no_term_reset = true;
                    char *new_body = config_entry_editor(selected_menu_entry->name, selected_menu_entry->body);
                    if (new_body == NULL)
                        goto refresh;
                    selected_menu_entry->body = new_body;
                    goto autoboot;
                }
                break;
            }
            case 'c':
            case 'C': {
                reset_term();
                console();
                goto refresh;
            }
        }
    }
}

noreturn void boot(char *config) {
    char *cmdline = config_get_value(config, 0, "KERNEL_CMDLINE");
    if (!cmdline) {
        cmdline = config_get_value(config, 0, "CMDLINE");
    }
    if (!cmdline) {
        cmdline = "";
    }

    char *proto = config_get_value(config, 0, "PROTOCOL");
    if (proto == NULL) {
        panic(true, "Boot protocol not specified for this entry");
    }

    if (!strcmp(proto, "stivale1") || !strcmp(proto, "stivale") || !strcmp(proto, "stivale2")) {
        print("The stivale and stivale2 protocols are no longer supported as of Limine 4.x\n");
        print("Please notify kernel maintainers to move to the Limine boot protocol or\n");
        print("roll back to Limine 3.x.\n\n");
    } else if (!strcmp(proto, "limine")) {
        limine_load(config, cmdline);
    } else if (!strcmp(proto, "linux")) {
#if defined (__x86_64__) || defined (__i386__)
        linux_load(config, cmdline);
#else
        print("TODO: Linux is not available on aarch64.\n\n");
#endif
    } else if (!strcmp(proto, "multiboot1") || !strcmp(proto, "multiboot")) {
#if defined (__x86_64__) || defined (__i386__)
        multiboot1_load(config, cmdline);
#else
        print("Multiboot 1 is not available on aarch64.\n\n");
#endif
    } else if (!strcmp(proto, "multiboot2")) {
#if defined (__x86_64__) || defined (__i386__)
        multiboot2_load(config, cmdline);
#else
        print("Multiboot 2 is not available on aarch64.\n\n");
#endif
    } else if (!strcmp(proto, "chainload_next")) {
        chainload_next(config);
    } else if (!strcmp(proto, "chainload")) {
        chainload(config);
    }

    panic(true, "Incorrect protocol specified for kernel.");
}
