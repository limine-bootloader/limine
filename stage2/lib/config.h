#ifndef __LIB__CONFIG_H__
#define __LIB__CONFIG_H__

#include <stddef.h>
#include <stdbool.h>

int init_config(int drive, int part);
int config_get_entry_name(char *ret, size_t index, size_t limit);
int config_set_entry(size_t index);
char *config_get_value(char *buf, size_t index, size_t limit, const char *key);
bool config_resolve_uri(char *uri, char **resource, char **root, char **path);

#endif
