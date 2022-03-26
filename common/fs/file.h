#ifndef __FS__FILE_H__
#define __FS__FILE_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>
#if uefi == 1
#  include <efi.h>
#endif

#define PATH_MAX 4096

bool fs_get_guid(struct guid *guid, struct volume *part);

struct file_handle {
    bool       is_memfile;
    bool       readall;
    struct volume *vol;
    char       path[PATH_MAX];
    void      *fd;
    void     (*read)(void *fd, void *buf, uint64_t loc, uint64_t count);
    void     (*close)(void *fd);
    uint64_t   size;
#if uefi == 1
    EFI_HANDLE efi_part_handle;
#endif
    bool pxe;
    uint32_t pxe_ip;
    uint16_t pxe_port;
};

struct file_handle *fopen(struct volume *part, const char *filename);
void fread(struct file_handle *fd, void *buf, uint64_t loc, uint64_t count);
void fclose(struct file_handle *fd);
void *freadall(struct file_handle *fd, uint32_t type);

#endif
