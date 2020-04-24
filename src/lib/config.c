#include <stddef.h>
#include <lib/config.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <fs/file.h>

#define SEPARATOR '\n'
#define MAX_CONFIG_SIZE 4096

static char *config_addr;

int init_config(int drive, int part) {
    struct file_handle f;

    if (fopen(&f, drive, part, "/qloader2.cfg")) {
        if (fopen(&f, drive, part, "/boot/qloader2.cfg")) {
            return -1;
        }
    }

    if (f.size >= MAX_CONFIG_SIZE) {
        panic("Config file is too big!\n");
    }

    config_addr = balloc(MAX_CONFIG_SIZE);
    memset(config_addr, 0, MAX_CONFIG_SIZE);

    fread(&f, config_addr, 0, f.size);

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
        if (*(p - 2) != '\n')
            i--;
    }

    config_addr = p;

    while (*p != ':' && *p)
        p++;

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
