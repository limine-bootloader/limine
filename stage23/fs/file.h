#ifndef __FS__FILE_H__
#define __FS__FILE_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>
#if uefi == 1
#  include <efi.h>
#endif

bool fs_get_guid(struct guid *guid, struct volume *part);

struct file_handle {
    bool       is_memfile;
    void      *fd;
    int      (*read)(void *fd, void *buf, uint64_t loc, uint64_t count);
    uint64_t   size;
#if uefi == 1
    EFI_HANDLE efi_part_handle;
#endif
};

int fopen(struct file_handle *ret, struct volume *part, const char *filename);
int fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count);
void *freadall(struct file_handle *fd, uint32_t type);

#endif
