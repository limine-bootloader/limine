#ifndef __DRIVERS__DISK_H__
#define __DRIVERS__DISK_H__

#include <stddef.h>
#include <stdint.h>
#include <lib/mbr.h>

int read(int drive, void *buffer, uint64_t loc, uint64_t count);
int read_partition(int drive, struct mbr_part *part, void *buffer, uint64_t loc, uint64_t count);

#endif
