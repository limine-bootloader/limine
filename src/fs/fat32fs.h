#pragma once

#include <stdint.h>

#include <lib/part.h>

enum fat32_file_attributes {
    FAT32_FILE_ATTRIB_READ_ONLY = (1 << 0),
    FAT32_FILE_ATTRIB_HIDDEN = (1 << 1),
    FAT32_FILE_ATTRIB_SYSTEM = (1 << 2),
    FAT32_FILE_ATTRIB_VOLUME_ID = (1 << 3),
    FAT32_FILE_ATTRIB_DIRECTORY = (1 << 4),
    FAT32_FILE_ATTRIB_ARCHIVE = (1 << 5),
    FAT32_FILE_ATTRIB_LFN = ((1 << 3) | (1 << 2) | (1 << 1) | (1 << 0))
};

struct fat32_directory_entry {
    uint8_t file_name[11]; ///< Short (8.3) file name.
    uint8_t attributes; ///< See `fat32_file_attributes`.
    uint8_t reserved;
    uint8_t creation_time_fine;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t accessed_date;
    uint16_t first_cluster_high; ///< Bits 31..16 of the first cluster number.
    uint16_t modification_time;
    uint16_t modification_date;
    uint16_t first_cluster_low; ///< Bits 15..0 of the first cluster number.
    uint32_t file_size; ///< Size of the file in bytes.
} __attribute__((packed));

struct fat32_file_handle {
    int disk;
    struct part part;

    uint16_t sector_size; ///< Bytes per sector.
    uint16_t reserved_sector_count; ///< Number of reserved sectors.
    uint32_t sector_count; ///< Number of sectors in the partition.
    uint32_t cluster_size; ///< Number of bytes per cluster.

    uint32_t fat_offset; ///< Offset to the first FAT from the beginning of a partition.
    uint32_t fat_size; ///< Length of an allocation table in bytes.

    uint32_t root_directory_offset; ///< Offset to the first root directory entry.

    uint32_t* allocation_map;
    uint32_t allocation_map_size;

    struct fat32_directory_entry directory_entry;
};

/// \brief Check whether the partition is FAT32 formatted.
///
/// \param disk Number of the disk (eg. 0x80).
/// \param partition Partition number.
///
/// \return 1 if the partition is FAT32 formatted, 0 if it is not.
int fat32_check_signature(int disk, int partition);

int fat32_open(struct fat32_file_handle* ret, int disk, int partition, const char* filename);
int fat32_read(struct fat32_file_handle* file, void* buf, uint64_t loc, uint64_t count);
