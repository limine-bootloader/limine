#include <fs/echfs.h>
#include <stdint.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <drivers/disk.h>

struct echfs_identity_table {
    uint8_t jmp[4];
    uint8_t signature[8];
    uint64_t block_count;
    uint64_t dir_length;
    uint64_t block_size;
} __attribute__((packed));

#define ROOT_DIR_ID  (~((uint64_t)0))
#define END_OF_CHAIN (~((uint64_t)0))
#define FILE_TYPE    0

#define CACHE_ADDR   ((uint8_t *)(0x20000))

static int cache_block(struct echfs_file_handle *file, uint64_t block) {
    // Load the file.
    uint64_t block_val = file->dir_entry.payload;
    for (uint64_t i = 0; i < block; i++) {
        if (block_val == END_OF_CHAIN)
            return -1;

        // Read the next block.
        read_partition(file->disk, &file->mbr_part, &block_val, file->alloc_table_offset + block_val * sizeof(uint64_t),
sizeof(uint64_t));
    }

    if (block_val == END_OF_CHAIN)
        return -1;

    return read_partition(file->disk, &file->mbr_part, CACHE_ADDR, block_val * file->block_size, file->block_size);
}

int echfs_read(struct echfs_file_handle *file, void *buf, uint64_t loc, uint64_t count) {
    for (uint64_t progress = 0; progress < count;) {
        /* cache the block */
        uint64_t block = (loc + progress) / file->block_size;

        int ret = cache_block(file, block);
        if (ret)
            return ret;

        uint64_t chunk = count - progress;
        uint64_t offset = (loc + progress) % file->block_size;
        if (chunk > file->block_size - offset)
            chunk = file->block_size - offset;

        memcpy(buf + progress, &CACHE_ADDR[offset], chunk);
        progress += chunk;
    }

    return 0;
}

int echfs_open(struct echfs_file_handle *ret, int disk, int partition, const char *filename) {
    ret->disk = disk;

    mbr_get_part(&ret->mbr_part, disk, partition);

    struct echfs_identity_table id_table;
    read_partition(disk, &ret->mbr_part, &id_table, 0, sizeof(struct echfs_identity_table));

    if (strncmp(id_table.signature, "_ECH_FS_", 8)) {
        print("echfs: signature invalid\n", filename);
        return -1;
    }

    ret->block_size         = id_table.block_size;
    ret->block_count        = id_table.block_count;
    ret->dir_length         = id_table.dir_length * ret->block_size;
    ret->alloc_table_size   = DIV_ROUNDUP(ret->block_count * sizeof(uint64_t), ret->block_size) * ret->block_size;
    ret->alloc_table_offset = 16 * ret->block_size;
    ret->dir_offset         = ret->alloc_table_offset + ret->alloc_table_size;

    // Find the file in the root dir.
    for (uint64_t i = 0; i < ret->dir_length; i += sizeof(struct echfs_dir_entry)) {
        read_partition(disk, &ret->mbr_part, &ret->dir_entry, i + ret->dir_offset, sizeof(struct echfs_dir_entry));

        if (!ret->dir_entry.parent_id) {
            break;
        }

        if (!strcmp(filename, ret->dir_entry.name) &&
            ret->dir_entry.parent_id == ROOT_DIR_ID &&
            ret->dir_entry.type == FILE_TYPE) {
            return 0;
        }
    }

    print("echfs: file %s not found\n", filename);
    return -1;
}
