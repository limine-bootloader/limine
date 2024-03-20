#ifndef DRIVERS__DISK_H__
#define DRIVERS__DISK_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>

#if defined (UEFI)

#include <efi.h>

struct volume *disk_volume_from_efi_handle(EFI_HANDLE efi_handle);

#endif

enum {
    DISK_SUCCESS,
    DISK_NO_MEDIA,
    DISK_FAILURE
};

void disk_create_index(void);
int disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count);

#endif
