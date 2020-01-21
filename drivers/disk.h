#ifndef __DISK_H__
#define __DISK_H__

#include <stddef.h>
#include <stdint.h>

int read_sector(int drive, void *buffer, uint64_t lba, uint64_t count);
int read(int drive, void *buffer, uint64_t loc, uint64_t count);

#endif
