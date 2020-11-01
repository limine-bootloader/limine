#include <stddef.h>
#include <stdint.h>
#include <fs/file.h>
#include <fs/echfs.h>
#include <fs/ext2.h>
#include <fs/fat32.h>
#include <lib/blib.h>
#include <mm/pmm.h>
#include <lib/part.h>

bool fs_get_guid(struct guid *guid, struct part *part) {
    if (echfs_check_signature(part)) {
        return echfs_get_guid(guid, part);
    }
    if (ext2_check_signature(part)) {
        return ext2_get_guid(guid, part);
    }

    return false;
}

int fopen(struct file_handle *ret, int disk, int partition, const char *filename) {
    struct part part;
    if (get_part(&part, disk, partition)) {
        panic("Invalid partition");
    }

    if (echfs_check_signature(&part)) {
        struct echfs_file_handle *fd = conv_mem_alloc(sizeof(struct echfs_file_handle));

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

    if (ext2_check_signature(&part)) {
        struct ext2_file_handle *fd = conv_mem_alloc(sizeof(struct ext2_file_handle));

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

    if (fat32_check_signature(&part)) {
        struct fat32_file_handle *fd = conv_mem_alloc(sizeof(struct fat32_file_handle));

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
