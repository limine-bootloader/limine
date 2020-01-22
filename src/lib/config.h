#ifndef __LIB__CONFIG_H__
#define __LIB__CONFIG_H__

#include <stddef.h>

char *config_get_value(char *buf, size_t limit, const char *config, const char *key);

#endif
