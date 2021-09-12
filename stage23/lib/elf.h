#ifndef __LIB__ELF_H__
#define __LIB__ELF_H__

#include <stdint.h>
#include <stdbool.h>
#include <fs/file.h>

#define FIXED_HIGHER_HALF_OFFSET_64 ((uint64_t)0xffffffff80000000)

#define ELF_PF_X 1
#define ELF_PF_W 2
#define ELF_PF_R 4

struct elf_range {
    uint64_t base;
    uint64_t length;
    uint64_t permissions;
};

struct elf_section_hdr_info {
    uint32_t section_hdr_size;
    uint32_t section_entry_size;
    uint32_t str_section_idx;
    uint32_t num;
    void* section_hdrs;
};

int elf_bits(uint8_t *elf);

int elf64_load(uint8_t *elf, uint64_t *entry_point, uint64_t *top, uint64_t *_slide, uint32_t alloc_type, bool kaslr, bool use_paddr, struct elf_range **ranges, uint64_t *ranges_count);
int elf64_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit, uint64_t slide);
struct elf_section_hdr_info* elf64_section_hdr_info(uint8_t *elf);

int elf32_load(uint8_t *elf, uint32_t *entry_point, uint32_t *top, uint32_t alloc_type);
int elf32_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit);
struct elf_section_hdr_info* elf32_section_hdr_info(uint8_t *elf);

#endif
