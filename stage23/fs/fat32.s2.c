#include <fs/fat32.h>
#include <lib/blib.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/print.h>
#include <mm/pmm.h>
#include <stdbool.h>

#define FAT32_LFN_MAX_ENTRIES 20
#define FAT32_LFN_MAX_FILENAME_LENGTH (FAT32_LFN_MAX_ENTRIES * 13 + 1)

#define FAT32_VALID_SIGNATURE_1 0x28
#define FAT32_VALID_SIGNATURE_2 0x29
#define FAT32_VALID_SYSTEM_IDENTIFIER "FAT32   "
#define FAT16_VALID_SYSTEM_IDENTIFIER "FAT16   "
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

static int fat32_init_context(struct fat32_context* context, struct volume *part) {
    context->part = part;

    struct fat32_bpb bpb;
    volume_read(context->part, &bpb, 0, sizeof(struct fat32_bpb));

    if (strncmp(bpb.system_identifier, FAT32_VALID_SYSTEM_IDENTIFIER, SIZEOF_ARRAY(bpb.system_identifier)) == 0) {
        if (bpb.signature == FAT32_VALID_SIGNATURE_1 || bpb.signature == FAT32_VALID_SIGNATURE_2) {
            context->type = 32;
            goto valid;
        }
    }

    if (strncmp((((void *)&bpb) + 0x36), FAT16_VALID_SYSTEM_IDENTIFIER, SIZEOF_ARRAY(bpb.system_identifier)) == 0) {
        context->type = 16;
        goto valid;
    }

    return 1;

valid:
    context->sectors_per_cluster = bpb.sectors_per_cluster;
    context->reserved_sectors = bpb.reserved_sectors;
    context->number_of_fats = bpb.fats_count;
    context->hidden_sectors = bpb.hidden_sectors_count;
    context->sectors_per_fat = context->type == 32 ? bpb.sectors_per_fat_32 : bpb.sectors_per_fat_16;
    context->root_directory_cluster = bpb.root_directory_cluster;
    context->fat_start_lba = bpb.reserved_sectors;
    context->root_entries = bpb.directory_entries_count;
    context->root_start = context->reserved_sectors + context->number_of_fats * context->sectors_per_fat;
    context->root_size = DIV_ROUNDUP(context->root_entries * sizeof(struct fat32_directory_entry), FAT32_SECTOR_SIZE);
    switch (context->type) {
        case 16:
            context->data_start_lba = context->root_start + context->root_size;
            break;
        case 32:
            context->data_start_lba = context->root_start;
            break;
        default:
            __builtin_unreachable();
    }

    return 0;
}

static int read_cluster_from_map(struct fat32_context *context, uint32_t cluster, uint32_t *out) {
    switch (context->type) {
        case 16:
            *out = 0;
            volume_read(context->part, out, context->fat_start_lba * FAT32_SECTOR_SIZE + cluster * sizeof(uint16_t), sizeof(uint16_t));
            break;
        case 32:
            volume_read(context->part, out, context->fat_start_lba * FAT32_SECTOR_SIZE + cluster * sizeof(uint32_t), sizeof(uint32_t));
            *out &= 0x0fffffff;
            break;
        default:
            __builtin_unreachable();
    }

    return 0;
}

static uint32_t *cache_cluster_chain(struct fat32_context *context,
                                     uint32_t initial_cluster,
                                     size_t *_chain_length) {
    uint32_t cluster_limit = (context->type == 16 ? 0xffef    : 0)
                           | (context->type == 32 ? 0xfffffef : 0);
    if (initial_cluster < 0x2 || initial_cluster > cluster_limit)
        return NULL;
    uint32_t cluster = initial_cluster;
    size_t chain_length;
    for (chain_length = 1; ; chain_length++) {
        read_cluster_from_map(context, cluster, &cluster);
        if (cluster < 0x2 || cluster > cluster_limit)
            break;
    }
    uint32_t *cluster_chain = ext_mem_alloc(chain_length * sizeof(uint32_t));
    cluster = initial_cluster;
    for (size_t i = 0; i < chain_length; i++) {
        cluster_chain[i] = cluster;
        read_cluster_from_map(context, cluster, &cluster);
    }
    *_chain_length = chain_length;
    return cluster_chain;
}

static bool read_cluster_chain(struct fat32_context *context,
                               uint32_t *cluster_chain,
                               void *buf, uint64_t loc, uint64_t count) {
    size_t block_size = context->sectors_per_cluster * FAT32_SECTOR_SIZE;
    for (uint64_t progress = 0; progress < count;) {
        uint64_t block = (loc + progress) / block_size;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % block_size;
        if (chunk > block_size - offset)
            chunk = block_size - offset;

        uint64_t base = ((uint64_t)context->data_start_lba + (cluster_chain[block] - 2) * context->sectors_per_cluster) * FAT32_SECTOR_SIZE;
        volume_read(context->part, buf + progress, base + offset, chunk);

        progress += chunk;
    }

    return true;
}

// Copy ucs-2 characters to char*
static void fat32_lfncpy(char* destination, const void* source, unsigned int size) {
    for (unsigned int i = 0; i < size; i++) {
        // ignore high bytes
        *(((uint8_t*) destination) + i) = *(((uint8_t*) source) + (i * 2));
    }
}

static bool fat32_filename_to_8_3(char *dest, const char *src) {
    int i = 0, j = 0;
    bool ext = false;

    for (size_t i = 0; i < 8+3; i++)
        dest[i] = ' ';

    while (src[i]) {
        if (src[i] == '.') {
            if (ext) {
                // This is a double extension here, just give up.
                return false;
            }
            ext = true;
            j = 8;
            i++;
            continue;
        }
        if (j >= 8+3 || (j >= 8 && !ext)) {
            // Filename too long, give up.
            return false;
        }
        dest[j++] = toupper(src[i++]);
    }

    return true;
}

static int fat32_open_in(struct fat32_context* context, struct fat32_directory_entry* directory, struct fat32_directory_entry* file, const char* name) {
    size_t block_size = context->sectors_per_cluster * FAT32_SECTOR_SIZE;
    char current_lfn[FAT32_LFN_MAX_FILENAME_LENGTH] = {0};

    size_t dir_chain_len;
    struct fat32_directory_entry *directory_entries;

    if (directory != NULL) {
        uint32_t current_cluster_number = directory->cluster_num_high << 16 | directory->cluster_num_low;

        uint32_t *directory_cluster_chain = cache_cluster_chain(context, current_cluster_number, &dir_chain_len);

        if (directory_cluster_chain == NULL)
            return -1;

        directory_entries = ext_mem_alloc(dir_chain_len * block_size);

        if (!read_cluster_chain(context, directory_cluster_chain, directory_entries, 0, dir_chain_len * block_size))
            return -1;
    } else {
        dir_chain_len = DIV_ROUNDUP(context->root_entries * sizeof(struct fat32_directory_entry), block_size);

        directory_entries = ext_mem_alloc(dir_chain_len * block_size);

        volume_read(context->part, directory_entries, context->root_start * FAT32_SECTOR_SIZE, context->root_entries * sizeof(struct fat32_directory_entry));
    }

    for (size_t i = 0; i < (dir_chain_len * block_size) / sizeof(struct fat32_directory_entry); i++) {
        if (directory_entries[i].file_name_and_ext[0] == 0x00) {
            // no more entries here
            break;
        }

        if (directory_entries[i].attribute == FAT32_LFN_ATTRIBUTE) {
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

            if (lfn_index != 0)
                continue;

            // remove trailing spaces
            for (int j = SIZEOF_ARRAY(current_lfn) - 2; j >= -1; j--) {
                if (j == -1 || current_lfn[j] != ' ') {
                    current_lfn[j + 1] = 0;
                    break;
                }
            }

            if (!strcmp(current_lfn, name)) {
                *file = directory_entries[i+1];
                return 0;
            }
        } else {
            char fn[8+3];
            if (!fat32_filename_to_8_3(fn, name)) {
                continue;
            }
            if (!strncmp(directory_entries[i].file_name_and_ext, fn, 8+3)) {
                *file = directory_entries[i];
                return 0;
            }
        }
    }

    // file not found
    return -1;
}

int fat32_check_signature(struct volume *part) {
    struct fat32_context context;
    return fat32_init_context(&context, part) == 0;
}

int fat32_open(struct fat32_file_handle* ret, struct volume *part, const char* path) {
    struct fat32_context context;
    int r = fat32_init_context(&context, part);

    if (r) {
        print("fat32: context init failure (%d)\n", r);
        return r;
    }

    struct fat32_directory_entry _current_directory;
    struct fat32_directory_entry *current_directory;
    struct fat32_directory_entry current_file;
    unsigned int current_index = 0;
    char current_part[FAT32_LFN_MAX_FILENAME_LENGTH];

    // skip trailing slashes
    while (path[current_index] == '/') {
        current_index++;
    }

    // walk down the directory tree
    switch (context.type) {
        case 16:
            current_directory = NULL;
            break;
        case 32:
            _current_directory.cluster_num_low = context.root_directory_cluster & 0xFFFF;
            _current_directory.cluster_num_high = context.root_directory_cluster >> 16;
            current_directory = &_current_directory;
            break;
        default:
            __builtin_unreachable();
    }

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

        if ((r = fat32_open_in(&context, current_directory, &current_file, current_part)) != 0) {
            return r;
        }

        if (expect_directory) {
            _current_directory = current_file;
            current_directory = &_current_directory;
        } else {
            ret->context = context;
            ret->first_cluster = current_file.cluster_num_high << 16 | current_file.cluster_num_low;
            ret->size_clusters = DIV_ROUNDUP(current_file.file_size_bytes, FAT32_SECTOR_SIZE);
            ret->size_bytes = current_file.file_size_bytes;
            uint32_t file_cluster = current_file.cluster_num_high << 16 | current_file.cluster_num_low;
            size_t file_chain_len;
            ret->cluster_chain = cache_cluster_chain(&context, file_cluster, &file_chain_len);
            return 0;
        }
    }
}

int fat32_read(struct fat32_file_handle* file, void* buf, uint64_t loc, uint64_t count) {
    return !read_cluster_chain(&file->context, file->cluster_chain, buf, loc, count);
}
