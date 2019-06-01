#ifndef __DISK_H__
#define __DISK_H__

#include <stdint.h>

int read_sector(int, int, int, uint8_t *);

#endif
