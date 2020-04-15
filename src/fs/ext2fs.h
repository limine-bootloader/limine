#ifndef __FS_EXT2FS_H__
#define __FS_EXT2FS_H__

#include <stdint.h>
#include <stddef.h>
#include <drivers/disk.h>
#include <lib/libc.h>
#include <lib/blib.h>
#include <lib/part.h>

struct ext2fs_file_handle {
    int drive;
    struct part part;
    int inode_num;
    int size;
};

int ext2fs_check_signature(int drive, int partition);

int ext2fs_open(struct ext2fs_file_handle *ret, int drive, int partition, const char* filename);
int ext2fs_read(struct ext2fs_file_handle *file, void* buf, uint64_t loc, uint64_t count);

#endif
