#include <stddef.h>
#include <stdint.h>
#include <fs/file.h>
#include <fs/echfs.h>
#include <fs/ext2.h>
#include <fs/fat32.h>
#include <lib/blib.h>

int fopen(struct file_handle *ret, int disk, int partition, const char *filename) {
    if (echfs_check_signature(disk, partition)) {
        struct echfs_file_handle *fd = balloc(sizeof(struct echfs_file_handle));

        int r = echfs_open(fd, disk, partition, filename);
        if (r)
            return r;

        ret->fd        = (void *)fd;
        ret->read      = (void *)echfs_read;
        ret->disk      = disk;
        ret->partition = partition;
        ret->size      = fd->dir_entry.size;

        return 0;
    }

    if (ext2_check_signature(disk, partition)) {
        struct ext2_file_handle *fd = balloc(sizeof(struct ext2_file_handle));

        int r = ext2_open(fd, disk, partition, filename);
        if (r)
            return r;

        ret->fd        = (void *)fd;
        ret->read      = (void *)ext2_read;
        ret->disk      = disk;
        ret->partition = partition;
        ret->size      = fd->size;

        return 0;
    }

    if (fat32_check_signature(disk, partition)) {
        struct fat32_file_handle *fd = balloc(sizeof(struct fat32_file_handle));

        int r = fat32_open(fd, disk, partition, filename);

        if (r)
            return r;

        ret->fd        = (void *)fd;
        ret->read      = (void *)fat32_read;
        ret->disk      = disk;
        ret->partition = partition;
        ret->size      = fd->size_bytes;

        return 0;
    }

    return -1;
}

int fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count) {
    return fd->read(fd->fd, buf, loc, count);
}
