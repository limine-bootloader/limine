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

struct file_handle *fopen(struct volume *part, const char *filename) {
    struct file_handle *ret = ext_mem_alloc(sizeof(struct file_handle));

    ret->is_memfile = false;
    ret->readall = false;

#if bios == 1
    if (part->pxe) {
        if (!tftp_open(ret, 0, 69, filename)) {
            goto fail;
        }
        return ret;
    }
#endif

#if uefi == 1
    ret->efi_part_handle = part->efi_part_handle;
#endif

    if (iso9660_check_signature(part)) {
        struct iso9660_file_handle *fd = ext_mem_alloc(sizeof(struct iso9660_file_handle));

        if (!iso9660_open(fd, part, filename)) {
            goto fail;
        }

        ret->fd = (void *)fd;
        ret->read = (void *)iso9660_read;
        ret->close = (void *)iso9660_close;
        ret->size = fd->size;

        return ret;
    }

    if (echfs_check_signature(part)) {
        struct echfs_file_handle *fd = ext_mem_alloc(sizeof(struct echfs_file_handle));

        if (!echfs_open(fd, part, filename)) {
            goto fail;
        }

        ret->fd = (void *)fd;
        ret->read = (void *)echfs_read;
        ret->close = (void *)echfs_close;
        ret->size = fd->dir_entry.size;

        return ret;
    }

    if (ext2_check_signature(part)) {
        struct ext2_file_handle *fd = ext_mem_alloc(sizeof(struct ext2_file_handle));

        if (!ext2_open(fd, part, filename)) {
            goto fail;
        }

        ret->fd = (void *)fd;
        ret->read = (void *)ext2_read;
        ret->close = (void *)ext2_close;
        ret->size = fd->size;

        return ret;
    }

    if (fat32_check_signature(part)) {
        struct fat32_file_handle *fd = ext_mem_alloc(sizeof(struct fat32_file_handle));

        if (!fat32_open(fd, part, filename)) {
            goto fail;
        }

        ret->fd = (void *)fd;
        ret->read = (void *)fat32_read;
        ret->close = (void *)fat32_close;
        ret->size = fd->size_bytes;

        return ret;
    }

fail:
    pmm_free(ret, sizeof(struct file_handle));
    return NULL;
}

void fclose(struct file_handle *fd) {
    if (fd->is_memfile) {
        if (fd->readall == false) {
            pmm_free(fd->fd, fd->size);
        }
    } else {
        fd->close(fd->fd);
    }
    pmm_free(fd, sizeof(struct file_handle));
}

void fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count) {
    if (fd->is_memfile) {
        memcpy(buf, fd->fd + loc, count);
    } else {
        fd->read(fd->fd, buf, loc, count);
    }
}

void *freadall(struct file_handle *fd, uint32_t type) {
    if (fd->is_memfile) {
        memmap_alloc_range((uint64_t)(size_t)fd->fd, ALIGN_UP(fd->size, 4096), type, false, true, false, false);
        fd->readall = true;
        return fd->fd;
    } else {
        void *ret = ext_mem_alloc_type(fd->size, type);
        fd->read(fd->fd, ret, 0, fd->size);
        return ret;
    }
}
