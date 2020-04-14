#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stdint.h>
<<<<<<< HEAD
#include <lib/blib.h>

int elf_load(FILE *fd, uint64_t *entry_point, uint64_t *top);
int elf_load_section(FILE *fd, void *buffer, const char *name, size_t limit);
=======
#include <fs/file.h>

int elf_load(struct file_handle *fd, uint64_t *entry_point, uint64_t *top);
int elf_load_section(struct file_handle *fd, void *buffer, const char *name, size_t limit);
>>>>>>> upstream/master

#endif
