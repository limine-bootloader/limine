#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stdint.h>
#include <fs/echfs.h>

int elf_load(struct echfs_file_handle *fd, uint64_t *entry_point);
int elf_load_section(struct echfs_file_handle *fd, void *buffer, const char *name);

#endif
