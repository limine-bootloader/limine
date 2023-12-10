#include <stddef.h>
#include <stdint.h>
#include <fs/file.h>
#include <fs/fat32.h>
#include <fs/iso9660.h>
#include <lib/print.h>
#include <lib/misc.h>
#include <mm/pmm.h>
#include <lib/part.h>
#include <lib/libc.h>
#include <pxe/tftp.h>

char *fs_get_label(struct volume *part) {
    char *ret;

    if ((ret = fat32_get_label(part)) != NULL) {
        return ret;
    }

    return NULL;
}

bool fs_get_guid(struct guid *guid, struct volume *part) {
    (void)guid; (void)part;

    return false;
}

bool case_insensitive_fopen = false;

struct file_handle *fopen(struct volume *part, const char *filename) {
    size_t filename_new_len = strlen(filename) + 2;
    char *filename_new = ext_mem_alloc(filename_new_len);

    if (filename[0] != '/') {
        filename_new[0] = '/';
        strcpy(&filename_new[1], filename);
    } else {
        strcpy(filename_new, filename);
    }

    filename = filename_new;

    struct file_handle *ret;

    if (part->pxe) {
        if ((ret = tftp_open(part, "", filename)) == NULL) {
            return NULL;
        }
        return ret;
    }

    if ((ret = iso9660_open(part, filename)) != NULL) {
        goto success;
    }
    if ((ret = fat32_open(part, filename)) != NULL) {
        goto success;
    }

    return NULL;

success:
    ret->path = (char *)filename;
    ret->path_len = filename_new_len;

    return ret;
}

void fclose(struct file_handle *fd) {
    if (fd->is_memfile) {
        if (fd->readall == false) {
            pmm_free(fd->fd, fd->size);
        }
    } else {
        fd->close(fd);
    }
    pmm_free(fd->path, fd->path_len);
    pmm_free(fd, sizeof(struct file_handle));
}

void fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count) {
    if (fd->is_memfile) {
        memcpy(buf, fd->fd + loc, count);
    } else {
        fd->read(fd, buf, loc, count);
    }
}

void *freadall(struct file_handle *fd, uint32_t type) {
    return freadall_mode(fd, type, false);
}

void *freadall_mode(struct file_handle *fd, uint32_t type, bool allow_high_allocs) {
    if (fd->is_memfile) {
        if (fd->readall) {
            return fd->fd;
        }
        memmap_alloc_range((uint64_t)(size_t)fd->fd, ALIGN_UP(fd->size, 4096), type, 0, true, false, false);
        fd->readall = true;
        return fd->fd;
    } else {
        void *ret = ext_mem_alloc_type_aligned_mode(fd->size, type, 4096, allow_high_allocs);
        fd->read(fd, ret, 0, fd->size);
        fd->close(fd);
        fd->fd = ret;
        fd->readall = true;
        fd->is_memfile = true;
        return ret;
    }
}
