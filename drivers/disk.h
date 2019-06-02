#ifndef __DISK_H__
#define __DISK_H__

#include <stddef.h>
#include <stdint.h>

int read_sector(int, void *, uint64_t, uint64_t);
int read(int, void *, uint64_t, uint64_t);

#endif
