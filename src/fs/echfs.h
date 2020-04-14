#ifndef __FS__ECHFS_H__
#define __FS__ECHFS_H__

#include <stdint.h>
#include <lib/mbr.h>

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

struct echfs_file_handle {
    int disk;
    struct mbr_part mbr_part;
    uint64_t block_size;
    uint64_t block_count;
    uint64_t dir_length;
    uint64_t alloc_table_size;
    uint64_t alloc_table_offset;
    uint64_t dir_offset;
    struct echfs_dir_entry dir_entry;
};

int echfs_open(struct echfs_file_handle *ret, int disk, int partition, const char *filename);
int echfs_read(struct echfs_file_handle *file, void *buf, uint64_t loc, uint64_t count);

#endif
