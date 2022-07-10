#ifndef __PROTOS__CHAINLOAD_H__
#define __PROTOS__CHAINLOAD_H__

void chainload(char *config);

#if uefi == 1
#include <fs/file.h>
void efi_chainload_file(struct file_handle *image);
#endif

#if bios == 1
#include <lib/part.h>
void bios_chainload_volume(struct volume *v);
#endif

#endif
