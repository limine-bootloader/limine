#include <fs/echfs.h>
#include <stdint.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/print.h>
#include <drivers/disk.h>
#include <stdbool.h>
#include <mm/pmm.h>

struct echfs_identity_table {
    uint8_t  jmp[4];
    char     signature[8];
    uint64_t block_count;
    uint64_t dir_length;
    uint64_t block_size;
    uint32_t reserved;
    struct guid guid;
} __attribute__((packed));

#define ROOT_DIR_ID  (~((uint64_t)0))
#define END_OF_CHAIN (~((uint64_t)0))
#define FILE_TYPE    0
#define DIR_TYPE     1

static int read_block(struct echfs_file_handle *file, void *buf, uint64_t block, uint64_t offset, uint64_t count) {
    return read_partition(file->disk, &file->part, buf, (file->alloc_map[block] * file->block_size) + offset, count);
}

int echfs_read(struct echfs_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    for (uint64_t progress = 0; progress < count;) {
        uint64_t block = (loc + progress) / file->block_size;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % file->block_size;
        if (chunk > file->block_size - offset)
            chunk = file->block_size - offset;

        read_block(file, buf + progress, block, offset, chunk);
        progress += chunk;
    }

    return 0;
}

int echfs_check_signature(struct part *part) {
    struct echfs_identity_table id_table;
    read_partition(part->drive, part, &id_table, 0, sizeof(struct echfs_identity_table));

    if (strncmp(id_table.signature, "_ECH_FS_", 8)) {
        return 0;
    }

    return 1;
}

bool echfs_get_guid(struct guid *guid, struct part *part) {
    struct echfs_identity_table id_table;
    read_partition(part->drive, part, &id_table, 0, sizeof(struct echfs_identity_table));

    if (strncmp(id_table.signature, "_ECH_FS_", 8)) {
        return false;
    }

    *guid = id_table.guid;

    return true;
}

int echfs_open(struct echfs_file_handle *ret, int disk, int partition, const char *path) {
    const char *fullpath = path;

    ret->disk = disk;

    if (get_part(&ret->part, disk, partition)) {
        panic("Invalid partition");
    }

    struct echfs_identity_table id_table;
    read_partition(disk, &ret->part, &id_table, 0, sizeof(struct echfs_identity_table));

    if (strncmp(id_table.signature, "_ECH_FS_", 8)) {
        print("echfs: signature invalid\n");
        return -1;
    }

    ret->block_size         = id_table.block_size;
    ret->block_count        = id_table.block_count;
    ret->dir_length         = id_table.dir_length * ret->block_size;
    ret->alloc_table_size   = DIV_ROUNDUP(ret->block_count * sizeof(uint64_t), ret->block_size) * ret->block_size;
    ret->alloc_table_offset = 16 * ret->block_size;
    ret->dir_offset         = ret->alloc_table_offset + ret->alloc_table_size;

    // Find the file in the root dir.
    uint64_t wanted_parent = ROOT_DIR_ID;
    bool     last_elem     = false;

next:;
    char wanted_name[128];
    for (; *path == '/'; path++);
    for (int i = 0; ; i++, path++) {
        if (*path == '\0' || *path == '/') {
            if (*path == '\0')
                last_elem = true;
            wanted_name[i] = '\0';
            path++;
            break;
        }
        wanted_name[i] = *path;
    }

    for (uint64_t i = 0; i < ret->dir_length; i += sizeof(struct echfs_dir_entry)) {
        read_partition(disk, &ret->part, &ret->dir_entry, i + ret->dir_offset, sizeof(struct echfs_dir_entry));

        if (!ret->dir_entry.parent_id) {
            break;
        }

        if (!strcmp(wanted_name, ret->dir_entry.name) &&
            ret->dir_entry.parent_id == wanted_parent &&
            ret->dir_entry.type == (last_elem ? FILE_TYPE : DIR_TYPE)) {
            if (last_elem) {
                goto found;
            } else {
                wanted_parent = ret->dir_entry.payload;
                goto next;
            }
        }
    }

    print("echfs: file %s not found\n", fullpath);
    return -1;

found:;
    // Load the allocation map.
    uint64_t file_block_count = DIV_ROUNDUP(ret->dir_entry.size, ret->block_size);

    ret->alloc_map = conv_mem_alloc(file_block_count * sizeof(uint64_t));

    ret->alloc_map[0] = ret->dir_entry.payload;
    for (uint64_t i = 1; i < file_block_count; i++) {
        // Read the next block.
        read_partition(ret->disk, &ret->part,
            &ret->alloc_map[i],
            ret->alloc_table_offset + ret->alloc_map[i-1] * sizeof(uint64_t),
            sizeof(uint64_t));
    }

    return 0;
}
