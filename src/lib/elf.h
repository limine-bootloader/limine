#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stdint.h>
#include <fs/file.h>

#define FIXED_HIGHER_HALF_OFFSET_64 ((uint64_t)0xffffffff80000000)

int elf_bits(struct file_handle *fd);

int elf64_load(struct file_handle *fd, uint64_t *entry_point, uint64_t *top, uint64_t slide);
int elf64_load_section(struct file_handle *fd, void *buffer, const char *name, size_t limit, uint64_t slide);

int elf32_load(struct file_handle *fd, uint32_t *entry_point, uint32_t *top);
int elf32_load_section(struct file_handle *fd, void *buffer, const char *name, size_t limit);

#endif
