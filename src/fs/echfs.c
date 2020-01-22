#include <fs/echfs.h>
#include <stdint.h>
#include <lib/libc.h>
#include <lib/print.h>

struct echfs_identity_table {
    uint8_t jmp[4];
    uint8_t signature[8];
    uint64_t block_count;
    uint64_t dir_length;
    uint64_t block_size;
} __attribute__((packed));

struct echfs_dir_entry {
    uint64_t parent_id;
    uint8_t type;
    char name[201];
    uint64_t atime;
    uint64_t mtime;
    uint16_t perms;
    uint16_t owner;
    uint16_t group;
    uint64_t ctime;
    uint64_t payload;
    uint64_t size;
} __attribute__((packed));

#define ROOT_DIR_ID  (~((uint64_t)0))
#define END_OF_CHAIN (~((uint64_t)0))
#define FILE_TYPE    0

int load_echfs_file(int disk, int partition, void *buffer, const char *filename) {
    // Read the S U P E R B L O C K.
    struct echfs_identity_table id_table;
    read_partition(disk, partition, &id_table, 0, sizeof(struct echfs_identity_table));

    // Check the signature.
    if (strncmp(id_table.signature, "_ECH_FS_", 8)) {
        return -1;
    }

    // Variables from the table.
    const uint64_t block_size  = id_table.block_size;
    const uint64_t block_count = id_table.block_count;
    const uint64_t dir_length  = id_table.dir_length;

    // Calculate the offset, jumping the Allocation table and reserved blocks.
    int offset = (16 * block_size) + (block_count * sizeof(uint64_t) / block_size);

    // Find the file in the root dir.
    struct echfs_dir_entry entry;
    for (int i = offset; i < dir_length * block_size; i += sizeof(struct echfs_dir_entry)) {
        read_partition(disk, partition, &entry, i, sizeof(struct echfs_dir_entry));

        if (!strcmp(filename, entry.name) && entry.parent_id == ROOT_DIR_ID && entry.type == FILE_TYPE) {
            goto FOUND;
        }
    }

    return -1;

    FOUND:;
    // Load the file.
    for (uint64_t i = entry.payload; i != END_OF_CHAIN;) {
        // Read block.
        read_partition(disk, partition, buffer, i * block_size, block_size);
        buffer += block_size;

        // Read the next block.
        read_partition(disk, partition, &i, (16 * block_size) + i * sizeof(uint64_t), sizeof(uint64_t));
    }

    return 0;
}
