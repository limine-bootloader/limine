#include <fs/ntfs.h>

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

int ntfs_check_signature(struct volume *part) {
    struct ntfs_bpb bpb = { 0 };
    if (volume_read(part, &bpb, 0, sizeof(bpb))) {
        return 1;
    }

    //
    // validate the bpb
    //

    if (strncmp(bpb.oem, "NTFS    ", SIZEOF_ARRAY(bpb.oem))) {
        return 1;
    }

    if (bpb.sector_totals != 0) {
        return 1;
    }

    if (bpb.large_sectors_count != 0) {
        return 1;
    }

    if (bpb.sectors_count_64 == 0) {
        return 1;
    }

    // this is a valid ntfs sector
    return 0;
}

int ntfs_open(struct ntfs_file_handle *ret, struct volume *part, const char *path) {
    (void)ret;
    (void)part;
    (void)path;
    return 1;
}

int ntfs_read(struct ntfs_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    (void)file;
    (void)buf;
    (void)loc;
    (void)count;
    return 1;
}
