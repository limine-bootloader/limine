#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stdint.h>
#include <lib/blib.h>

int elf_load(FILE *fd, uint64_t *entry_point);
int elf_load_section(FILE *fd, void *buffer, const char *name, size_t limit);

#endif
