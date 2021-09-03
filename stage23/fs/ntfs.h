#ifndef __FS__NTFS_H__
#define __FS__NTFS_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>
#include <lib/blib.h>

struct ntfs_file_handle {
    struct volume *part;
    uint32_t size_bytes;
};

int ntfs_check_signature(struct volume *part);

int ntfs_open(struct ntfs_file_handle *ret, struct volume *part, const char *path);
int ntfs_read(struct ntfs_file_handle *file, void *buf, uint64_t loc, uint64_t count);

#endif