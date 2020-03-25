#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stdint.h>
#include <fs/echfs.h>

int elf_load(struct echfs_file_handle *fd, uint64_t *entry_point);

#endif
