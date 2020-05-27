#include <stddef.h>
#include <lib/config.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <fs/file.h>

#define SEPARATOR '\n'

static char *config_addr;

int init_config(int drive, int part) {
    struct file_handle f;

    if (fopen(&f, drive, part, "/qloader2.cfg")) {
        if (fopen(&f, drive, part, "/boot/qloader2.cfg")) {
            return -1;
        }
    }

    config_addr = balloc(f.size + 1);
    memset(config_addr, 0, f.size + 1);

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
