#ifndef __DRIVERS__DISK_H__
#define __DRIVERS__DISK_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <lib/part.h>

#if uefi == 1

#include <efi.h>

struct volume *disk_volume_from_efi_handle(EFI_HANDLE *efi_handle);

#endif

void disk_create_index(void);
bool disk_read_sectors(struct volume *volume, void *buf, uint64_t block, size_t count);

#endif
