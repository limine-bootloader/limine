#include <stddef.h>
#include <stdint.h>
#include <fs/file.h>
#include <fs/echfs.h>
#include <fs/ext2.h>
#include <fs/fat32.h>
#include <fs/iso9660.h>
#include <lib/print.h>
#include <lib/blib.h>
#include <mm/pmm.h>
#include <lib/part.h>
#include <lib/libc.h>
#include <pxe/tftp.h>

bool fs_get_guid(struct guid *guid, struct volume *part) {
    if (echfs_check_signature(part)) {
        return echfs_get_guid(guid, part);
    }
    if (ext2_check_signature(part)) {
        return ext2_get_guid(guid, part);
    }

    return false;
}

int fopen(struct file_handle *ret, struct volume *part, const char *filename) {
    ret->is_memfile = false;

#if bios == 1
    if (part->pxe) {
        int r = tftp_open(ret, 0, 69, filename);
        if (r)
            return r;

        return 0;
    }
#endif

#if uefi == 1
    ret->efi_part_handle = part->efi_part_handle;
#endif

    if (iso9660_check_signature(part)) {
        struct iso9660_file_handle *fd = ext_mem_alloc(sizeof(struct iso9660_file_handle));

        int r = iso9660_open(fd, part, filename);
        if (r)
            return r;

        ret->fd = (void *)fd;
        ret->read = (void *)iso9660_read;
        ret->size = fd->size;

        return 0;
    }

    if (echfs_check_signature(part)) {
        struct echfs_file_handle *fd = ext_mem_alloc(sizeof(struct echfs_file_handle));

        int r = echfs_open(fd, part, filename);
        if (r)
            return r;

        ret->fd   = (void *)fd;
        ret->read = (void *)echfs_read;
        ret->size = fd->dir_entry.size;

        return 0;
    }

    if (ext2_check_signature(part)) {
        struct ext2_file_handle *fd = ext_mem_alloc(sizeof(struct ext2_file_handle));

        int r = ext2_open(fd, part, filename);
        if (r)
            return r;

        ret->fd   = (void *)fd;
        ret->read = (void *)ext2_read;
        ret->size = fd->size;

        return 0;
    }

    if (fat32_check_signature(part)) {
        struct fat32_file_handle *fd = ext_mem_alloc(sizeof(struct fat32_file_handle));

        int r = fat32_open(fd, part, filename);

        if (r)
            return r;

        ret->fd   = (void *)fd;
        ret->read = (void *)fat32_read;
        ret->size = fd->size_bytes;

        return 0;
    }

    return -1;
}

int fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count) {
    if (fd->is_memfile) {
        memcpy(buf, fd->fd + loc, count);
        return 0;
    } else {
        return fd->read(fd->fd, buf, loc, count);
    }
}

void *freadall(struct file_handle *fd, uint32_t type) {
    if (fd->is_memfile) {
        memmap_alloc_range((uint64_t)(size_t)fd->fd, ALIGN_UP(fd->size, 4096), type, false, true, false, false);
        return fd->fd;
    } else {
        void *ret = ext_mem_alloc_type(fd->size, type);
        if (fd->read(fd->fd, ret, 0, fd->size)) {
            panic("freadall error");
        }
        return ret;
    }
}
