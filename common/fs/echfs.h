#ifndef __FS__ECHFS_H__
#define __FS__ECHFS_H__

#include <stdint.h>
#include <stdbool.h>
#include <lib/part.h>
#include <lib/blib.h>

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
    struct volume *part;
    uint64_t block_size;
    uint64_t block_count;
    uint64_t dir_length;
    uint64_t alloc_table_size;
    uint64_t alloc_table_offset;
    uint64_t dir_offset;
    uint64_t file_block_count;
    uint64_t *alloc_map;
    struct echfs_dir_entry dir_entry;
};

int echfs_check_signature(struct volume *part);
bool echfs_get_guid(struct guid *guid, struct volume *part);

bool echfs_open(struct echfs_file_handle *ret, struct volume *part, const char *filename);
void echfs_read(struct echfs_file_handle *file, void *buf, uint64_t loc, uint64_t count);
void echfs_close(struct echfs_file_handle *file);

#endif
