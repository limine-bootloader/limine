#include <stddef.h>
#include <lib/config.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <fs/echfs.h>

#define SEPARATOR '\n'
#define CONFIG_NAME "qloader2.cfg"
#define MAX_CONFIG_SIZE 4096

static char *config_addr;

int init_config(int drive, int part) {
    struct echfs_file_handle f;

    if (echfs_open(&f, drive, part, CONFIG_NAME)) {
        return -1;
    }

    if (f.dir_entry.size >= 4096) {
        print("Config file is too big!\n");
        for (;;);
    }

    config_addr = balloc(MAX_CONFIG_SIZE);
    memset(config_addr, 0, MAX_CONFIG_SIZE);

    echfs_read(&f, config_addr, 0, f.dir_entry.size);

    return 0;
}

char *config_get_value(char *buf, size_t limit, const char *key) {
    if (!limit || !buf || !key)
        return NULL;

    size_t key_len = strlen(key);

    for (size_t i = 0; config_addr[i]; i++) {
        if (!strncmp(&config_addr[i], key, key_len) && config_addr[i + key_len] == '=') {
            if (i && config_addr[i - 1] != SEPARATOR)
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
