#ifndef __DRIVERS__DISK_H__
#define __DRIVERS__DISK_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>

#if defined (uefi)

#include <efi.h>

bool disk_volume_from_efi_handle(struct volume *ret, EFI_HANDLE *efi_handle);

#endif

size_t disk_create_index(struct volume **ret);
bool disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count);

#endif
