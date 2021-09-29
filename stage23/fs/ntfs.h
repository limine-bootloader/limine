#ifndef __FS__NTFS_H__
#define __FS__NTFS_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>
#include <lib/blib.h>

struct ntfs_bpb {
    uint8_t jump[3];
    char oem[8];
    uint16_t bytes_per_sector;
    uint8_t sectors_per_cluster;
    uint16_t reserved_sectors;
    uint8_t fats_count;
    uint16_t directory_entries_count;
    uint16_t sector_totals;
    uint8_t media_descriptor_type;
    uint16_t sectors_per_fat_16;
    uint16_t sectors_per_track;
    uint16_t heads_count;
    uint32_t hidden_sectors_count;
    uint32_t large_sectors_count;
    
    // ntfs 
    uint32_t sectors_per_fat_32;
    uint64_t sectors_count_64;
    uint64_t mft_cluster;
} __attribute__((packed));

struct ntfs_file_handle {
    struct volume *part;

    struct ntfs_bpb bpb;

    // file record sizes
    uint64_t file_record_size;
    uint64_t sectors_per_file_record;

    // MFT info, the offset and its runlist
    uint64_t mft_offset;
    uint8_t mft_run_list[256];

    // the runlist, resident index and attribute list of the 
    // current open file/directory
    uint8_t run_list[128];
    uint8_t resident_index_size;
    uint8_t resident_index[256];

    // info about the current file
    uint32_t size_bytes;
};

int ntfs_check_signature(struct volume *part);

int ntfs_open(struct ntfs_file_handle *ret, struct volume *part, const char *path);
int ntfs_read(struct ntfs_file_handle *file, void *buf, uint64_t loc, uint64_t count);

#endif