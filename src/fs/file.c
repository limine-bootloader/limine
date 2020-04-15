#include <stddef.h>
#include <stdint.h>
#include <fs/file.h>
#include <fs/echfs.h>
#include <fs/ext2fs.h>
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

    if (ext2fs_check_signature(disk, partition)) {
        struct ext2fs_file_handle *fd = balloc(sizeof(struct ext2fs_file_handle));

        int r = ext2fs_open(fd, disk, partition, filename);
        if (!r)
            return 1;

        ret->fd        = (void *)fd;
        ret->read      = (void *)ext2fs_read;
        ret->disk      = disk;
        ret->partition = partition;
        ret->size      = fd->size;

        return 0;
    }

    print("fs: Could not determine the file system of disk %u partition %u",
          disk, partition);

    return -1;
}

int fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count) {
    return fd->read(fd->fd, buf, loc, count);
}
