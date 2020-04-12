#ifndef __LIB__CONFIG_H__
#define __LIB__CONFIG_H__

#include <stddef.h>
#include <lib/mbr.h>

int init_config(int drive, struct mbr_part part);
char *config_get_value(char *buf, size_t index, size_t limit, const char *key);

#endif
