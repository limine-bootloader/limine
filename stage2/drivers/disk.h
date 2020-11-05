#ifndef __DRIVERS__DISK_H__
#define __DRIVERS__DISK_H__

#include <stddef.h>
#include <stdint.h>
#include <lib/part.h>

struct bios_drive_params {
    uint16_t buf_size;
    uint16_t info_flags;
    uint32_t cyl;
    uint32_t heads;
    uint32_t sects;
    uint64_t lba_count;
    uint16_t bytes_per_sect;
    uint32_t edd;
} __attribute__((packed));

int disk_get_sector_size(int drive);
int read(int drive, void *buffer, uint64_t loc, uint64_t count);
int read_partition(int drive, struct part *part, void *buffer, uint64_t loc, uint64_t count);

#endif
