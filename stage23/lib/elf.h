#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stdint.h>
#include <fs/file.h>

#define FIXED_HIGHER_HALF_OFFSET_64 ((uint64_t)0xffffffff80000000)

int elf_bits(uint8_t *elf);

int elf64_load(uint8_t *elf, uint64_t *entry_point, uint64_t *slide, uint32_t alloc_type);
int elf64_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit, uint64_t slide);

int elf32_load(uint8_t *elf, uint32_t *entry_point, uint32_t alloc_type);
int elf32_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit);

#endif
