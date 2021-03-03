#ifndef __DRIVERS__DISK_H__
#define __DRIVERS__DISK_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
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
bool disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count);

#endif
