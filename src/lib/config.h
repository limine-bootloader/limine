#ifndef __LIB__CONFIG_H__
#define __LIB__CONFIG_H__

#include <stddef.h>

int init_config(int drive, int part);
char *config_get_value(char *buf, size_t index, size_t limit, const char *key);

#endif
