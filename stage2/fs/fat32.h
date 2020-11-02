#ifndef __FS__FAT32_H__
#define __FS__FAT32_H__

#include <stdint.h>
#include <lib/part.h>

struct fat32_context {
    int drive;
    struct part part;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t number_of_fats;
    uint32_t hidden_sectors;
    uint32_t sectors_per_fat;
    uint32_t root_directory_cluster;
    uint32_t fat_start_lba;
    uint32_t data_start_lba;
};

struct fat32_file_handle {
    struct fat32_context context;
    uint32_t first_cluster;
    uint32_t size_bytes;
    uint32_t size_clusters;
};

int fat32_check_signature(struct part *part);

int fat32_open(struct fat32_file_handle *ret, struct part *part, const char *path);
int fat32_read(struct fat32_file_handle *file, void *buf, uint64_t loc, uint64_t count);

#endif
