#include <stddef.h>
#include <stdbool.h>
#include <lib/config.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <mm/pmm.h>
#include <fs/file.h>
#include <lib/print.h>
#include <pxe/tftp.h>

#define SEPARATOR '\n'

bool config_ready = false;

static char *config_addr;

int init_config_disk(struct part *part) {
    struct file_handle f;

    if (fopen(&f, part, "/limine.cfg")) {
        if (fopen(&f, part, "/boot/limine.cfg")) {
            return -1;
        }
    }

    size_t config_size = f.size + 1;
    config_addr = conv_mem_alloc(config_size);

    fread(&f, config_addr, 0, f.size);

    return init_config(config_size);
}

int init_config_pxe(void) {
    struct tftp_file_handle cfg;
    if (tftp_open(&cfg, 0, 69, "limine.cfg")) {
        return -1;
    }
    config_addr = conv_mem_alloc(cfg.file_size);
    tftp_read(&cfg, config_addr, 0, cfg.file_size);

    print("\nconfig: %s\n", config_addr);

    return init_config(cfg.file_size);
}

int init_config(size_t config_size) {
    // remove windows carriage returns, if any
    for (size_t i = 0; i < config_size; i++) {
        if (config_addr[i] == '\r') {
            for (size_t j = i; j < config_size - 1; j++)
                config_addr[j] = config_addr[j+1];
            config_size--;
        }
    }

    config_ready = true;

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
