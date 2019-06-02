#ifndef __DISK_H__
#define __DISK_H__

#include <stdint.h>

int read_sector(int, void *, uint64_t, size_t);
int read(int, void *, int, size_t);

#endif
