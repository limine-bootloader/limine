#ifndef __PROTOS__CHAINLOAD_H__
#define __PROTOS__CHAINLOAD_H__

#include <stdnoreturn.h>

noreturn void chainload(char *config);

#if defined (UEFI)
#include <fs/file.h>
noreturn void efi_chainload_file(char *config, struct file_handle *image);
#endif

#if defined (BIOS)
#include <lib/part.h>
void bios_chainload_volume(struct volume *v);
#endif

#endif
