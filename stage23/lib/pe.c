#include <stdbool.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/pe.h>
#include <lib/print.h>
#include <mm/pmm.h>

#define FIXED_HIGHER_HALF_OFFSET_64 ((uint64_t)0xffffffff80000000)

#define MZ_MAGIC 0x5a4d

#define PE_MAGIC         0x00004550
#define PE_MACHINE_I386  0x14c
#define PE_MACHINE_AMD64 0x8664

#define PE_OPT_MAGIC_PE32 0x10b
#define PE_OPT_MAGIC_PE64 0x20b

#define PE_SEC_UNINIT_DATA (1 << 7)

// Meant to be used by the functions
static bool pe_validate(uint8_t *pe, bool is_64) {
    struct pe_dos_header *dos_hdr = (struct pe_dos_header *) pe;
    if (dos_hdr->magic != MZ_MAGIC) {
        print("pe: Invalid MS-DOS MZ header magic\n");
        return false;
    }

    struct pe_header *pe_hdr = (struct pe_header *) (pe + dos_hdr->new_header_off);
    if (pe_hdr->magic != PE_MAGIC) {
        print("pe: Invalid PE header magic\n");
        return false;
    }

    if (is_64) {
        if (pe_hdr->machine != PE_MACHINE_AMD64) {
            print("pe: Invalid PE header machine (must be AMD64)\n");
            return false;
        }
        if (pe_hdr->optional_header_size == 0) {
            print("pe: No optional header");
            return false;
        }
        struct pe64_optional_header *pe64_opt_hdr = (struct pe64_optional_header *) (pe_hdr + 1);
        if (pe64_opt_hdr->magic != PE_OPT_MAGIC_PE64) {
            print("pe: Invalid PE optional header magic (must be PE64)\n");
            return false;
        }
    } else {
        if (pe_hdr->machine != PE_MACHINE_I386) {
            print("pe: Invalid PE header machine (must be i386)\n");
            return false;
        }
        if (pe_hdr->optional_header_size == 0) {
            print("pe: No optional header\n");
            return false;
        }
        struct pe32_optional_header *pe32_opt_hdr = (struct pe32_optional_header *) (pe_hdr + 1);
        if (pe32_opt_hdr->magic != PE_OPT_MAGIC_PE32) {
            print("pe: Invalid PE optional header magic (must be PE32)\n");
            return false;
        }
    }

    return true;
}

bool pe_detect(uint8_t *pe) {
    struct pe_dos_header *dos_hdr = (struct pe_dos_header *) pe;
    if (dos_hdr->magic != MZ_MAGIC)
        return false;
    
    struct pe_header *pe_hdr = (struct pe_header *) (pe + dos_hdr->new_header_off);
    if (pe_hdr->magic != PE_MAGIC)
        return false;

    return true;
}

int pe_bits(uint8_t *pe) {
    struct pe_dos_header *dos_hdr = (struct pe_dos_header *) pe;
    struct pe_header *pe_hdr = (struct pe_header *) (pe + dos_hdr->new_header_off);

    if (pe_hdr->machine == PE_MACHINE_I386)
        return 32;
    else if (pe_hdr->machine == PE_MACHINE_AMD64)
        return 64;
    else
        return -1;
}

int pe64_load(uint8_t *pe, uint64_t *entry_point, uint64_t *top, uint32_t alloc_type) {
    if (!pe_validate(pe, true)) {
        return 1;
    }

    struct pe_dos_header *dos_hdr = (struct pe_dos_header *) pe;
    struct pe_header *pe_hdr = (struct pe_header *) (pe + dos_hdr->new_header_off);
    struct pe64_optional_header *pe64_opt_hdr = (struct pe64_optional_header *) (pe_hdr + 1);

    uint16_t sections = pe_hdr->section_amount;
    uint64_t image_base = pe64_opt_hdr->image_base;
    uint64_t physical_base = image_base;
    if (image_base & (1ULL << 63))
        physical_base -= FIXED_HIGHER_HALF_OFFSET_64;

    *entry_point = image_base + pe64_opt_hdr->entry_point_addr;

    struct pe_section_header *section = (struct pe_section_header *) ((uint8_t *) pe64_opt_hdr + pe_hdr->optional_header_size);

    uint64_t this_top = 0;
    if (top)
        *top = this_top;

    for (uint16_t i = 0; i < sections; i++) {
        if (!memmap_alloc_range(physical_base + section->virtual_addr, section->virtual_size, alloc_type, true, false, false, false)) {
            panic("pe: Could not copy section %S", (const char *) &section->name, 8);
        }

        if (section->characteristics & PE_SEC_UNINIT_DATA)
            memset((void *) (uintptr_t) physical_base + section->virtual_addr, 0, section->virtual_size);
        else
            memcpy((void *) (uintptr_t) physical_base + section->virtual_addr, pe + section->data_ptr, section->virtual_size);

        this_top = physical_base + section->virtual_addr;
        if (top) {
            if (this_top > *top)
                *top = this_top;
        }

        section++;
    }

    return 0;
}

int pe64_load_section(uint8_t *pe, void *buffer, const char *name, size_t limit) {
    if (!pe_validate(pe, true))
        return 1;

    struct pe_dos_header *dos_hdr = (struct pe_dos_header *) pe;
    struct pe_header *pe_hdr = (struct pe_header *) (pe + dos_hdr->new_header_off);
    struct pe64_optional_header *pe64_opt_hdr = (struct pe64_optional_header *) (pe_hdr + 1);

    uint16_t sections = pe_hdr->section_amount;

    struct pe_section_header *section = (struct pe_section_header *) ((uint8_t *) pe64_opt_hdr + pe_hdr->optional_header_size);
    for (uint16_t i = 0; i < sections; i++) {
        if (!strcmp((const char *) &section->name, name)) {
            if (section->virtual_size > limit)
                return 3;
            if (section->virtual_size < limit)
                return 4;
            memcpy(buffer, pe + section->data_ptr, limit);
            return 0;
        }
        section++;
    }

    return 2;
}

int pe32_load(uint8_t *pe, uint32_t *entry_point, uint32_t *top, uint32_t alloc_type) {
    if (!pe_validate(pe, false))
        return 1;

    struct pe_dos_header *dos_hdr = (struct pe_dos_header *) pe;
    struct pe_header *pe_hdr = (struct pe_header *) (pe + dos_hdr->new_header_off);
    struct pe32_optional_header *pe32_opt_hdr = (struct pe32_optional_header *) (pe_hdr + 1);

    uint16_t sections = pe_hdr->section_amount;
    uint32_t image_base = pe32_opt_hdr->image_base;
    
    *entry_point = image_base + pe32_opt_hdr->entry_point_addr;

    struct pe_section_header *section = (struct pe_section_header *) ((uint8_t *) pe32_opt_hdr + pe_hdr->optional_header_size);

    uint32_t this_top = 0;
    if (top)
        *top = 0;

    for (uint16_t i = 0; i < sections; i++) {
        if (!memmap_alloc_range(image_base + section->virtual_addr, section->virtual_size, alloc_type, true, false, false, false))
            panic("pe: Could not copy section %S", (const char *) &section->name, 8);

        if (section->characteristics & PE_SEC_UNINIT_DATA)
            memset((void *) (uintptr_t) image_base + section->virtual_addr, 0, section->virtual_size);
        else
            memcpy((void *) (uintptr_t) image_base + section->virtual_addr, pe + section->data_ptr, section->virtual_size);

        this_top = image_base + section->virtual_addr;
        if (top) {
            if (this_top > *top)
                *top = this_top;
        }

        section++;
    }
    return 0;
}

int pe32_load_section(uint8_t *pe, void *buffer, const char *name, size_t limit) {
    if (!pe_validate(pe, false))
        return 1;

    struct pe_dos_header *dos_hdr = (struct pe_dos_header *) pe;
    struct pe_header *pe_hdr = (struct pe_header *) (pe + dos_hdr->new_header_off);
    struct pe32_optional_header *pe32_opt_hdr = (struct pe32_optional_header *) (pe_hdr + 1);

    uint16_t sections = pe_hdr->section_amount;

    struct pe_section_header *section = (struct pe_section_header *) ((uint8_t *) pe32_opt_hdr + pe_hdr->optional_header_size);
    for (uint16_t i = 0; i < sections; i++) {
        if (!strcmp((const char *) &section->name, name)) {
            if (section->virtual_size > limit)
                return 3;
            if (section->virtual_size < limit)
                return 4;
            memcpy(buffer, pe + section->data_ptr, limit);
            return 0;
        }
        section++;
    }

    return 2;
}
