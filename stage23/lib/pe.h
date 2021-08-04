#ifndef __LIB__PE_H__
#define __LIB__PE_H__

#include <stddef.h>
#include <stdint.h>

struct pe_dos_header {
    uint16_t magic;
    uint16_t bytes_last_page;
    uint16_t pages;
    uint16_t relocations;
    uint16_t header_size_paragraphs;
    uint16_t min_extra_paragraphs;
    uint16_t max_extra_paragraphs;
    uint16_t init_ss;
    uint16_t init_sp;
    uint16_t checksum;
    uint16_t init_ip;
    uint16_t init_cs;
    uint16_t rel_table_off;
    uint16_t overlay;
    uint16_t reserved0[4];
    uint16_t oem_id;
    uint16_t oem_info;
    uint16_t reserved1[10];
    uint32_t new_header_off;
} __attribute__((packed));

struct pe_header {
    uint32_t magic;
    uint16_t machine;
    uint16_t section_amount;
    uint32_t timestamp;
    uint32_t symbol_table_ptr;
    uint32_t symbol_amount;
    uint16_t optional_header_size;
    uint16_t characteristics;
} __attribute__((packed));

struct pe32_optional_header {
    uint16_t magic;
    uint8_t  linker_major_version;
    uint8_t  linker_minor_version;
    uint32_t code_size;
    uint32_t initialized_data_size;
    uint32_t uninitialized_data_size;
    uint32_t entry_point_addr;
    uint32_t code_base;
    uint32_t data_base;
    // All of these after, mostly Windows specific
    uint32_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t os_major_version;
    uint16_t os_minor_version;
    uint16_t image_major_version;
    uint16_t image_minor_version;
    uint16_t subsystem_major_version;
    uint16_t subsystem_minor_version;
    uint32_t win32_version;
    uint32_t image_size;
    uint32_t headers_size;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint32_t stack_reserve_size;
    uint32_t stack_commit_size;
    uint32_t heap_reserve_size;
    uint32_t heap_commit_size;
    uint32_t loader_flags;
    uint32_t rva_amount_sizes;
} __attribute__((packed));

struct pe64_optional_header {
    uint16_t magic;
    uint8_t  linker_major_version;
    uint8_t  linker_minor_version;
    uint32_t code_size;
    uint32_t initialized_data_size;
    uint32_t uninitialized_data_size;
    uint32_t entry_point_addr;
    uint32_t code_base;
    // All of these after, mostly Windows specific
    uint64_t image_base;
    uint32_t section_alignment;
    uint32_t file_alignment;
    uint16_t os_major_version;
    uint16_t os_minor_version;
    uint16_t image_major_version;
    uint16_t image_minor_version;
    uint16_t subsystem_major_version;
    uint16_t subsystem_minor_version;
    uint32_t win32_version;
    uint32_t image_size;
    uint32_t headers_size;
    uint32_t checksum;
    uint16_t subsystem;
    uint16_t dll_characteristics;
    uint64_t stack_reserve_size;
    uint64_t stack_commit_size;
    uint64_t heap_reserve_size;
    uint64_t heap_commit_size;
    uint32_t loader_flags;
    uint32_t rva_amount_sizes;
} __attribute__((packed));

struct pe_section_header {
    char name[8];
    union {
        uint32_t physical_addr; // Only valid for object files
        uint32_t virtual_size;
    };
    uint32_t virtual_addr;
    uint32_t aligned_size;
    uint32_t data_ptr;
    uint32_t rels_ptr;
    uint32_t lines_ptr;
    uint16_t relocations_amount;
    uint16_t line_numbers;
    uint32_t characteristics;
} __attribute__((packed));

bool pe_detect(uint8_t *pe);
int pe_bits(uint8_t *pe); // Assumes the PE is valid

int pe64_load(uint8_t *pe, uint64_t *entry_point, uint64_t *top, uint32_t alloc_type);
int pe64_load_section(uint8_t *pe, void *buffer, const char *name, size_t limit);

int pe32_load(uint8_t *pe, uint32_t *entry_point, uint32_t *top, uint32_t alloc_type);
int pe32_load_section(uint8_t *pe, void *buffer, const char *name, size_t limit);

#endif
