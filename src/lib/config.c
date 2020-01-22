#include <stddef.h>
#include <lib/config.h>
#include <lib/libc.h>

#define SEPARATOR '\n'

char *config_get_value(char *buf, size_t limit, const char *config, const char *key) {
    if (!limit || !buf)
        return NULL;

    size_t key_len = strlen(key);

    for (size_t i = 0; config[i]; i++) {
        if (!strncmp(&config[i], key, key_len) && config[i + key_len] == '=') {
            if (i && config[i - 1] != SEPARATOR)
                continue;
            i += key_len + 1;
            size_t j;
            for (j = 0; config[i + j] != SEPARATOR && config[i + j]; j++) {
                if (j == limit - 1)
                    break;
                buf[j] = config[i + j];
            }
            buf[j] = 0;
            return buf;
        }
    }

    return NULL;
}
