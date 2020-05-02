#include <fs/fat32.h>
#include <lib/blib.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <stdbool.h>

#define FAT32_LFN_MAX_ENTRIES 20
#define FAT32_LFN_MAX_FILENAME_LENGTH (FAT32_LFN_MAX_ENTRIES * 13 + 1)

#define FAT32_VALID_SIGNATURE_1 0x28
#define FAT32_VALID_SIGNATURE_2 0x29
#define FAT32_VALID_SYSTEM_IDENTIFIER "FAT32   "
#define FAT32_SECTOR_SIZE 512
#define FAT32_ATTRIBUTE_SUBDIRECTORY 0x10
#define FAT32_LFN_ATTRIBUTE 0x0F

struct fat32_bpb {
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
    uint32_t sectors_per_fat_32;
    uint16_t flags;
    uint16_t fat_version_number;
    uint32_t root_directory_cluster;
    uint16_t fs_info_sector;
    uint16_t backup_boot_sector;
    uint8_t reserved[12];
    uint8_t drive_number;
    uint8_t nt_flags;
    uint8_t signature;
    uint32_t volume_serial_number;
    char label[11];
    char system_identifier[8];
} __attribute__((packed));

struct fat32_directory_entry {
    char file_name_and_ext[8 + 3];
    uint8_t attribute;
    uint8_t file_data_1[8];
    uint16_t cluster_num_high;
    uint8_t file_data_2[4];
    uint16_t cluster_num_low;
    uint32_t file_size_bytes;
} __attribute__((packed));

struct fat32_lfn_entry {
    uint8_t sequence_number;
    char name1[10];
    uint8_t attribute;
    uint8_t type;
    uint8_t dos_checksum;
    char name2[12];
    uint16_t first_cluster;
    char name3[4];
} __attribute__((packed));

static int fat32_init_context(struct fat32_context* context, int disk, int partition) {
    context->drive = disk;
    get_part(&context->part, disk, partition);

    struct fat32_bpb bpb;
    read_partition(disk, &context->part, &bpb, 0, sizeof(struct fat32_bpb));

    if (bpb.signature != FAT32_VALID_SIGNATURE_1 && bpb.signature != FAT32_VALID_SIGNATURE_2) {
        return 1;
    }

    if (strncmp(bpb.system_identifier, FAT32_VALID_SYSTEM_IDENTIFIER, SIZEOF_ARRAY(bpb.system_identifier)) != 0) {
        return 1;
    }

    context->sectors_per_cluster = bpb.sectors_per_cluster;
    context->reserved_sectors = bpb.reserved_sectors;
    context->number_of_fats = bpb.fats_count;
    context->hidden_sectors = bpb.hidden_sectors_count;
    context->sectors_per_fat = bpb.sectors_per_fat_32;
    context->root_directory_cluster = bpb.root_directory_cluster;
    context->fat_start_lba = bpb.reserved_sectors + bpb.hidden_sectors_count;
    context->data_start_lba = context->fat_start_lba + bpb.fats_count * bpb.sectors_per_fat_32;

    return 0;
}

static int fat32_read_cluster_from_map(struct fat32_context* context, uint32_t cluster, uint32_t* out) {
    const uint32_t sector = cluster / (FAT32_SECTOR_SIZE / 4);
    const uint32_t offset = cluster % (FAT32_SECTOR_SIZE / 4);

    uint32_t clusters[FAT32_SECTOR_SIZE / sizeof(uint32_t)];
    int r = read_partition(context->drive, &context->part, &clusters[0], (context->fat_start_lba + sector) * FAT32_SECTOR_SIZE, sizeof(clusters));

    if (r) {
        return r;
    }

    *out = clusters[offset] & 0x0FFFFFFF;
    return 0;
}

static int fat32_load_fat_cluster_to_memory(struct fat32_context* context, uint32_t cluster_number, void* buffer, uint32_t offset, uint32_t limit) {
    const uint32_t sector = context->data_start_lba + (cluster_number - 2) * context->sectors_per_cluster;
    return read_partition(context->drive, &context->part, buffer, ((uint64_t) sector) * FAT32_SECTOR_SIZE + offset, limit);
}

// Copy ucs-2 characters to char*
static void fat32_lfncpy(char* destination, const void* source, unsigned int size) {
    for (unsigned int i = 0; i < size; i++) {
        // ignore high bytes
        *(((uint8_t*) destination) + i) = *(((uint8_t*) source) + (i * 2));
    }
}

static int fat32_open_in(struct fat32_context* context, struct fat32_directory_entry* directory, struct fat32_directory_entry* file, const char* name) {
    int error;
    uint32_t current_cluster_number = directory->cluster_num_high << 16 | directory->cluster_num_low;

    char current_lfn[FAT32_LFN_MAX_FILENAME_LENGTH] = {0};
    bool has_lfn = false;

    do {
        for (size_t sector_in_cluster = 0; sector_in_cluster < context->sectors_per_cluster; sector_in_cluster++) {
            struct fat32_directory_entry directory_entries[FAT32_SECTOR_SIZE / sizeof(struct fat32_directory_entry)];
            error = fat32_load_fat_cluster_to_memory(context, current_cluster_number, directory_entries, 0 * FAT32_SECTOR_SIZE, sizeof(directory_entries));

            if (error != 0) {
                return error;
            }

            for (unsigned int i = 0; i < SIZEOF_ARRAY(directory_entries); i++) {
                if (directory_entries[i].file_name_and_ext[0] == 0x00) {
                    // no more entries here
                    break;
                }

                if (directory_entries[i].attribute == FAT32_LFN_ATTRIBUTE) {
                    has_lfn = true;

                    struct fat32_lfn_entry* lfn = (struct fat32_lfn_entry*) &directory_entries[i];

                    if (lfn->sequence_number & 0b01000000) {
                        // this lfn is the first entry in the table, clear the lfn buffer
                        memset(current_lfn, ' ', sizeof(current_lfn));
                    }

                    const unsigned int lfn_index = ((lfn->sequence_number & 0b00011111) - 1U) * 13U;
                    if (lfn_index >= FAT32_LFN_MAX_ENTRIES * 13) {
                        continue;
                    }

                    fat32_lfncpy(current_lfn + lfn_index + 00, lfn->name1, 5);
                    fat32_lfncpy(current_lfn + lfn_index + 05, lfn->name2, 6);
                    fat32_lfncpy(current_lfn + lfn_index + 11, lfn->name3, 2);
                    continue;
                }

                if (has_lfn) {
                    // remove trailing spaces
                    for (int j = SIZEOF_ARRAY(current_lfn) - 2; j >= -1; j--) {
                        if (j == -1 || current_lfn[j] != ' ') {
                            current_lfn[j + 1] = 0;
                            break;
                        }
                    }
                }

                if ((has_lfn && strcmp(current_lfn, name) == 0) || strncmp(directory_entries[i].file_name_and_ext, name, 8 + 3) == 0) {
                    *file = directory_entries[i];
                    return 0;
                }

                if (has_lfn) {
                    has_lfn = false;
                }
            }
        }

        error = fat32_read_cluster_from_map(context, current_cluster_number, &current_cluster_number);

        if (error != 0) {
            return error;
        }
    } while (current_cluster_number >= 0x00000002 && current_cluster_number <= 0x0FFFFEF);

    // file not found
    return -1;
}

int fat32_check_signature(int disk, int partition) {
    struct fat32_context context;
    return fat32_init_context(&context, disk, partition) == 0;
}

int fat32_open(struct fat32_file_handle* ret, int disk, int partition, const char* path) {
    struct fat32_context context;
    int r = fat32_init_context(&context, disk, partition);

    if (r) {
        print("fat32: context init failure (%d)\n", r);
        return r;
    }

    struct fat32_directory_entry current_directory;
    struct fat32_directory_entry current_file;
    unsigned int current_index = 0;
    char current_part[FAT32_LFN_MAX_FILENAME_LENGTH];

    // skip trailing slashes
    while (path[current_index] == '/') {
        current_index++;
    }

    // walk down the directory tree
    current_directory.cluster_num_low = context.root_directory_cluster & 0xFFFF;
    current_directory.cluster_num_high = context.root_directory_cluster >> 16;

    for (;;) {
        bool expect_directory = false;

        for (unsigned int i = 0; i < SIZEOF_ARRAY(current_part); i++) {
            if (path[i + current_index] == 0) {
                memcpy(current_part, path + current_index, i);
                current_part[i] = 0;
                expect_directory = false;
                break;
            }

            if (path[i + current_index] == '/') {
                memcpy(current_part, path + current_index, i);
                current_part[i] = 0;
                current_index += i + 1;
                expect_directory = true;
                break;
            }
        }

        if ((r = fat32_open_in(&context, &current_directory, &current_file, current_part)) != 0) {
            print("fat32: file %s not found\n", path);
            return r;
        }

        if (expect_directory) {
            current_directory = current_file;
        } else {
            ret->context = context;
            ret->first_cluster = current_file.cluster_num_high << 16 | current_file.cluster_num_low;
            ret->size_clusters = DIV_ROUNDUP(current_file.file_size_bytes, FAT32_SECTOR_SIZE);
            ret->size_bytes = current_file.file_size_bytes;
            return 0;
        }
    }
}

int fat32_read(struct fat32_file_handle* file, void* buf, uint64_t loc, uint64_t count) {
    int r;
    uint32_t cluster_size = file->context.sectors_per_cluster * FAT32_SECTOR_SIZE;
    uint32_t current_cluster_number = file->first_cluster;

    // skip first clusters
    while (loc >= cluster_size) {
        r = fat32_read_cluster_from_map(&file->context, current_cluster_number, &current_cluster_number);

        if (r != 0) {
            print("fat32: failed to read cluster %x from map\n", current_cluster_number);
            return r;
        }

        loc -= cluster_size;
    }

    uint64_t read_total = 0;

    do {
        // find non-fragmented cluster chains to improve read performance
        uint32_t non_fragmented_clusters = 1;
        for (size_t i = 0 ; i < count / cluster_size; i++) {
            uint32_t next_cluster;

            r = fat32_read_cluster_from_map(&file->context, current_cluster_number + i, &next_cluster);

            if (r != 0) {
                print("fat32: failed to read cluster %x from map\n", current_cluster_number);
                return r;
            }

            if (next_cluster != current_cluster_number + i + 1) {
                break;
            }

            non_fragmented_clusters++;
        }

        // find largest read size
        uint64_t current_read = count;
        if (current_read > non_fragmented_clusters * cluster_size - loc) {
            current_read = non_fragmented_clusters * cluster_size - loc;
        }

        r = fat32_load_fat_cluster_to_memory(&file->context, current_cluster_number, buf + read_total, loc, current_read);

        if (r != 0) {
            print("fat32: failed to load cluster %x to memory\n", current_cluster_number);
            return r;
        }

        loc = 0;
        count -= current_read;
        read_total += current_read;

        if (count == 0) {
            return 0;
        }

        // fetch next cluster number
        r = fat32_read_cluster_from_map(&file->context, current_cluster_number, &current_cluster_number);

        if (r != 0) {
            print("fat32: failed to read cluster %x from map\n", current_cluster_number);
            return r;
        }
    } while (current_cluster_number >= 0x00000002 && current_cluster_number <= 0x0FFFFEF);

    print("fat32: read failed, unexpected end of cluster chain\n");
    return 0;
}
