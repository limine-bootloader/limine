#include <stddef.h>
#include <stdbool.h>
#include <lib/config.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <mm/pmm.h>
#include <fs/file.h>
#include <lib/print.h>
#include <pxe/tftp.h>

static bool config_get_entry_name(char *ret, size_t index, size_t limit);
static char *config_get_entry(size_t *size, size_t index);

#define SEPARATOR '\n'

bool config_ready = false;
no_unwind bool bad_config = false;

static char *config_addr;

int init_config_disk(struct volume *part) {
    struct file_handle *f;

    if ((f = fopen(part, "/limine.cfg")) == NULL
     && (f = fopen(part, "/boot/limine.cfg")) == NULL
     && (f = fopen(part, "/EFI/BOOT/limine.cfg")) == NULL) {
        return -1;
    }

    size_t config_size = f->size + 2;
    config_addr = ext_mem_alloc(config_size);

    fread(f, config_addr, 0, f->size);

    fclose(f);

    return init_config(config_size);
}

#if bios == 1
int init_config_pxe(void) {
    struct file_handle *f;
    if ((f = tftp_open(0, 69, "limine.cfg")) == NULL) {
        return -1;
    }

    size_t config_size = f->size + 2;
    config_addr = ext_mem_alloc(config_size);

    fread(f, config_addr, 0, f->size);

    return init_config(config_size);
}
#endif

#define NOT_CHILD      (-1)
#define DIRECT_CHILD   0
#define INDIRECT_CHILD 1

static int is_child(char *buf, size_t limit,
                    size_t current_depth, size_t index) {
    if (!config_get_entry_name(buf, index, limit))
        return NOT_CHILD;
    if (strlen(buf) < current_depth + 1)
        return NOT_CHILD;
    for (size_t j = 0; j < current_depth; j++)
        if (buf[j] != ':')
            return NOT_CHILD;
    if (buf[current_depth] == ':')
        return INDIRECT_CHILD;
    return DIRECT_CHILD;
}

static bool is_directory(char *buf, size_t limit,
                         size_t current_depth, size_t index) {
    switch (is_child(buf, limit, current_depth + 1, index + 1)) {
        default:
        case NOT_CHILD:
            return false;
        case INDIRECT_CHILD:
            bad_config = true;
            panic(true, "config: Malformed config file. Parentless child.");
        case DIRECT_CHILD:
            return true;
    }
}

static struct menu_entry *create_menu_tree(struct menu_entry *parent,
                                           size_t current_depth, size_t index) {
    struct menu_entry *root = NULL, *prev = NULL;

    for (size_t i = index; ; i++) {
        static char name[64];

        switch (is_child(name, 64, current_depth, i)) {
            case NOT_CHILD:
                return root;
            case INDIRECT_CHILD:
                continue;
            case DIRECT_CHILD:
                break;
        }

        struct menu_entry *entry = ext_mem_alloc(sizeof(struct menu_entry));

        if (root == NULL)
            root = entry;

        config_get_entry_name(name, i, 64);

        bool default_expanded = name[current_depth] == '+';

        strcpy(entry->name, name + current_depth + default_expanded);
        entry->parent = parent;

        size_t entry_size;
        char *config_entry = config_get_entry(&entry_size, i);
        entry->body = ext_mem_alloc(entry_size + 1);
        memcpy(entry->body, config_entry, entry_size);
        entry->body[entry_size] = 0;

        if (is_directory(name, 64, current_depth, i)) {
            entry->sub = create_menu_tree(entry, current_depth + 1, i + 1);
            entry->expanded = default_expanded;
        }

        char *comment = config_get_value(entry->body, 0, "COMMENT");
        if (comment != NULL) {
            entry->comment = comment;
        }

        if (prev != NULL)
            prev->next = entry;
        prev = entry;
    }
}

struct menu_entry *menu_tree = NULL;

struct macro {
    char name[1024];
    char value[2048];
    struct macro *next;
};

static struct macro *macros = NULL;

int init_config(size_t config_size) {
    // add trailing newline if not present
    config_addr[config_size - 2] = '\n';

    // remove windows carriage returns and spaces at the start of lines, if any
    for (size_t i = 0; i < config_size; i++) {
        size_t skip = 0;
        while ((config_addr[i + skip] == '\r')
            || ((!i || config_addr[i - 1] == '\n')
             && (config_addr[i + skip] == ' ' || config_addr[i + skip] == '\t'))) {
            skip++;
        }
        if (skip) {
            for (size_t j = i; j < config_size - skip; j++)
                config_addr[j] = config_addr[j + skip];
            config_size -= skip;
        }
    }

    // Load macros
    for (size_t i = 0; i < config_size;) {
        if ((config_size - i >= 3 && memcmp(config_addr + i, "\n${", 3) == 0)
         || (config_size - i >= 2 && i == 0 && memcmp(config_addr, "${", 2) == 0)) {
            struct macro *macro = ext_mem_alloc(sizeof(struct macro));

            i += i ? 3 : 2;
            size_t j;
            for (j = 0; config_addr[i] != '}' && config_addr[i] != '\n' && config_addr[i] != 0; j++, i++) {
                macro->name[j] = config_addr[i];
            }

            if (config_addr[i] == '\n' || config_addr[i] == 0 || config_addr[i+1] != '=') {
                continue;
            }
            i += 2;

            macro->name[j] = 0;

            for (j = 0; config_addr[i] != '\n' && config_addr[i] != 0; j++, i++) {
                macro->value[j] = config_addr[i];
            }
            macro->value[j] = 0;

            macro->next = macros;
            macros = macro;

            continue;
        }

        i++;
    }

    // Expand macros
    if (macros != NULL) {
        size_t new_config_size = config_size * 4;
        char *new_config = ext_mem_alloc(new_config_size);

        size_t i, in;
        for (i = 0, in = 0; i < config_size;) {
            if ((config_size - i >= 3 && memcmp(config_addr + i, "\n${", 3) == 0)
             || (config_size - i >= 2 && i == 0 && memcmp(config_addr, "${", 2) == 0)) {
                size_t orig_i = i;
                i += i ? 3 : 2;
                while (config_addr[i++] != '}') {
                    if (i >= config_size) {
                        bad_config = true;
                        panic(true, "config: Malformed macro usage");
                    }
                }
                if (config_addr[i] != '=') {
                    i = orig_i;
                    goto next;
                }
                continue;
            }

next:
            if (config_size - i >= 2 && memcmp(config_addr + i, "${", 2) == 0) {
                char *macro_name = ext_mem_alloc(1024);
                i += 2;
                size_t j;
                for (j = 0; config_addr[i] != '}' && config_addr[i] != '\n' && config_addr[i] != 0; j++, i++) {
                    macro_name[j] = config_addr[i];
                }
                if (config_addr[i] != '}') {
                    bad_config = true;
                    panic(true, "config: Malformed macro usage");
                }
                i++;
                macro_name[j] = 0;
                char *macro_value = "";
                struct macro *macro = macros;
                for (;;) {
                    if (macro == NULL) {
                        break;
                    }
                    if (strcmp(macro->name, macro_name) == 0) {
                        macro_value = macro->value;
                        break;
                    }
                    macro = macro->next;
                }
                pmm_free(macro_name, 1024);
                for (j = 0; macro_value[j] != 0; j++, in++) {
                    if (in >= new_config_size) {
                        goto overflow;
                    }
                    new_config[in] = macro_value[j];
                }
                continue;
            }

            if (in >= new_config_size) {
overflow:
                bad_config = true;
                panic(true, "config: Macro-induced buffer overflow");
            }
            new_config[in++] = config_addr[i++];
        }

        pmm_free(config_addr, config_size);

        config_addr = new_config;
        config_size = in;

        // Free macros
        struct macro *macro = macros;
        for (;;) {
            if (macro == NULL) {
                break;
            }
            struct macro *next = macro->next;
            pmm_free(macro, sizeof(struct macro));
            macro = next;
        }
    }

    config_ready = true;

    menu_tree = create_menu_tree(NULL, 1, 0);

    size_t s;
    char *c = config_get_entry(&s, 0);
    while (*c != ':') {
        c--;
    }
    if (c > config_addr) {
        c[-1] = 0;
    }

    return 0;
}

static bool config_get_entry_name(char *ret, size_t index, size_t limit) {
    if (!config_ready)
        return false;

    char *p = config_addr;

    for (size_t i = 0; i <= index; i++) {
        while (*p != ':') {
            if (!*p)
                return false;
            p++;
        }
        p++;
        if ((p - 1) != config_addr && *(p - 2) != '\n')
            i--;
    }

    p--;

    size_t i;
    for (i = 0; i < (limit - 1); i++) {
        if (p[i] == SEPARATOR)
            break;
        ret[i] = p[i];
    }

    ret[i] = 0;
    return true;
}

static char *config_get_entry(size_t *size, size_t index) {
    if (!config_ready)
        return NULL;

    char *ret;
    char *p = config_addr;

    for (size_t i = 0; i <= index; i++) {
        while (*p != ':') {
            if (!*p)
                return NULL;
            p++;
        }
        p++;
        if ((p - 1) != config_addr && *(p - 2) != '\n')
            i--;
    }

    do {
        p++;
    } while (*p != '\n');

    ret = p;

cont:
    while (*p != ':' && *p)
        p++;

    if (*p && *(p - 1) != '\n') {
        p++;
        goto cont;
    }

    *size = p - ret;

    return ret;
}

static const char *lastkey;

struct conf_tuple config_get_tuple(const char *config, size_t index,
                                   const char *key1, const char *key2) {
    struct conf_tuple conf_tuple;

    conf_tuple.value1 = config_get_value(config, index, key1);
    if (conf_tuple.value1 == NULL) {
        return (struct conf_tuple){0};
    }

    conf_tuple.value2 = config_get_value(lastkey, 0, key2);

    const char *lk1 = lastkey;

    const char *next_value1 = config_get_value(config, index + 1, key1);

    const char *lk2 = lastkey;

    if (conf_tuple.value2 != NULL && next_value1 != NULL) {
        if ((uintptr_t)lk1 > (uintptr_t)lk2) {
            conf_tuple.value2 = NULL;
        }
    }

    return conf_tuple;
}

char *config_get_value(const char *config, size_t index, const char *key) {
    if (!key || !config_ready)
        return NULL;

    if (config == NULL)
        config = config_addr;

    size_t key_len = strlen(key);

    for (size_t i = 0; config[i]; i++) {
        if (!strncmp(&config[i], key, key_len) && config[i + key_len] == '=') {
            if (i && config[i - 1] != SEPARATOR)
                continue;
            if (index--)
                continue;
            i += key_len + 1;
            size_t value_len;
            for (value_len = 0;
                 config[i + value_len] != SEPARATOR && config[i + value_len];
                 value_len++);
            char *buf = ext_mem_alloc(value_len + 1);
            memcpy(buf, config + i, value_len);
            lastkey = config + i;
            return buf;
        }
    }

    return NULL;
}
