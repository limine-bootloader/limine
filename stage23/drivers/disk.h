#ifndef __DRIVERS__DISK_H__
#define __DRIVERS__DISK_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>

size_t disk_create_index(struct volume **ret);
bool disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count);

#endif
