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

#if defined (BIOS)
// `boot_drive` is the drive stage 1 booted from, or 0 where it booted from no
// drive at all.
void disk_create_index(uint8_t boot_drive);
#elif defined (UEFI)
void disk_create_index(void);
#endif
int disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count);

#endif
