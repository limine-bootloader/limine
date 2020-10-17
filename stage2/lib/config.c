#include <stddef.h>
#include <stdbool.h>
#include <lib/config.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <mm/pmm.h>
#include <fs/file.h>

#define SEPARATOR '\n'

static char *config_addr;

int init_config(int drive, int part) {
    struct file_handle f;

    if (fopen(&f, drive, part, "/limine.cfg")) {
        if (fopen(&f, drive, part, "/boot/limine.cfg")) {
            return -1;
        }
    }

    size_t config_size = f.size + 1;
    config_addr = conv_mem_alloc(config_size);

    fread(&f, config_addr, 0, f.size);

    // remove windows carriage returns, if any
    for (size_t i = 0; i < config_size; i++) {
        if (config_addr[i] == '\r') {
            for (size_t j = i; j < config_size - 1; j++)
                config_addr[j] = config_addr[j+1];
            config_size--;
        }
    }

    return 0;
}

int config_get_entry_name(char *ret, size_t index, size_t limit) {
    char *p = config_addr;

    for (size_t i = 0; i <= index; i++) {
        while (*p != ':') {
            if (!*p)
                return -1;
            p++;
        }
        p++;
        if ((p - 1) != config_addr && *(p - 2) != '\n')
            i--;
    }

    size_t i;
    for (i = 0; i < (limit - 1); i++) {
        if (p[i] == SEPARATOR)
            break;
        ret[i] = p[i];
    }

    ret[i] = 0;
    return 0;
}

int config_set_entry(size_t index) {
    char *p = config_addr;

    for (size_t i = 0; i <= index; i++) {
        while (*p != ':') {
            if (!*p)
                return -1;
            p++;
        }
        p++;
        if ((p - 1) != config_addr && *(p - 2) != '\n')
            i--;
    }

    config_addr = p;

cont:
    while (*p != ':' && *p)
        p++;

    if (*p && *(p - 1) != '\n') {
        p++;
        goto cont;
    }

    *p = 0;

    return 0;
}

char *config_get_value(char *buf, size_t index, size_t limit, const char *key) {
    if (!limit || !buf || !key)
        return NULL;

    size_t key_len = strlen(key);

    for (size_t i = 0; config_addr[i]; i++) {
        if (!strncmp(&config_addr[i], key, key_len) && config_addr[i + key_len] == '=') {
            if (i && config_addr[i - 1] != SEPARATOR)
                continue;
            if (index--)
                continue;
            i += key_len + 1;
            size_t j;
            for (j = 0; config_addr[i + j] != SEPARATOR && config_addr[i + j]; j++) {
                if (j == limit - 1)
                    break;
                buf[j] = config_addr[i + j];
            }
            buf[j] = 0;
            return buf;
        }
    }

    return NULL;
}

// A URI takes the form of: resource://root/path
// The following function splits up a URI into its componenets
bool config_resolve_uri(char *uri, char **resource, char **root, char **path) {
    *resource = *root = *path = NULL;

    // Get resource
    for (size_t i = 0; ; i++) {
        if (strlen(uri + i) < 3)
            return false;

        if (!memcmp(uri + i, "://", 3)) {
            *resource = uri;
            uri[i] = 0;
            uri += i + 3;
            break;
        }
    }

    for (size_t i = 0; ; i++) {
        if (uri[i] == 0)
            return false;

        if (uri[i] == '/') {
            *root = uri;
            uri[i] = 0;
            uri += i + 1;
            break;
        }
    }

    if (*uri == 0)
        return false;

    *path = uri;

    return true;
}
