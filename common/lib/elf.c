#include <stdint.h>
#include <stddef.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <lib/print.h>
#include <lib/rand.h>
#include <mm/pmm.h>
#include <fs/file.h>

#define PT_LOAD     0x00000001
#define PT_DYNAMIC  0x00000002
#define PT_INTERP   0x00000003
#define PT_PHDR     0x00000006

#define DT_NULL     0x00000000
#define DT_NEEDED   0x00000001
#define DT_RELA     0x00000007
#define DT_RELASZ   0x00000008
#define DT_RELAENT  0x00000009

#define ABI_SYSV    0x00
#define ARCH_X86_64 0x3e
#define ARCH_X86_32 0x03
#define BITS_LE     0x01
#define ET_DYN      0x0003
#define SHT_RELA    0x00000004
#define R_X86_64_RELATIVE 0x00000008

/* Indices into identification array */
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7

struct elf32_hdr {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint32_t entry;
    uint32_t phoff;
    uint32_t shoff;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t phdr_size;
    uint16_t ph_num;
    uint16_t shdr_size;
    uint16_t sh_num;
    uint16_t shstrndx;
};

struct elf64_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf32_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
    uint32_t p_align;
};

struct elf32_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint32_t sh_flags;
    uint32_t sh_addr;
    uint32_t sh_offset;
    uint32_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint32_t sh_addralign;
    uint32_t sh_entsize;
};

struct elf64_rela {
    uint64_t r_addr;
    uint32_t r_info;
    uint32_t r_symbol;
    uint64_t r_addend;
};

struct elf64_dyn {
    uint64_t d_tag;
    uint64_t d_un;
};

int elf_bits(uint8_t *elf) {
    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), 20);

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return -1;
    }

    switch (hdr.machine) {
        case ARCH_X86_64:
            return 64;
        case ARCH_X86_32:
            return 32;
        default:
            return -1;
    }
}

static bool elf64_is_relocatable(uint8_t *elf, struct elf64_hdr *hdr) {
    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    // Find DYN segment
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr phdr;
        memcpy(&phdr, elf + (hdr->phoff + i * hdr->phdr_size),
               sizeof(struct elf64_phdr));

        if (phdr.p_type != PT_DYNAMIC) {
            continue;
        }

        for (uint16_t j = 0; j < phdr.p_filesz / sizeof(struct elf64_dyn); j++) {
            struct elf64_dyn dyn;
            memcpy(&dyn, elf + (phdr.p_offset + j * sizeof(struct elf64_dyn)),
                   sizeof(struct elf64_dyn));

            switch (dyn.d_tag) {
                case DT_RELA:
                    return true;
            }
        }
    }

    return false;
}

static int elf64_apply_relocations(uint8_t *elf, struct elf64_hdr *hdr, void *buffer, uint64_t vaddr, size_t size, uint64_t slide) {
    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    // Find DYN segment
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr phdr;
        memcpy(&phdr, elf + (hdr->phoff + i * hdr->phdr_size),
               sizeof(struct elf64_phdr));

        if (phdr.p_type != PT_DYNAMIC)
            continue;

        uint64_t rela_offset = 0;
        uint64_t rela_size = 0;
        uint64_t rela_ent = 0;
        for (uint16_t j = 0; j < phdr.p_filesz / sizeof(struct elf64_dyn); j++) {
            struct elf64_dyn dyn;
            memcpy(&dyn, elf + (phdr.p_offset + j * sizeof(struct elf64_dyn)),
                   sizeof(struct elf64_dyn));

            switch (dyn.d_tag) {
                case DT_RELA:
                    rela_offset = dyn.d_un;
                    break;
                case DT_RELAENT:
                    rela_ent = dyn.d_un;
                    break;
                case DT_RELASZ:
                    rela_size = dyn.d_un;
                    break;
            }
        }

        if (rela_offset == 0) {
            break;
        }

        if (rela_ent != sizeof(struct elf64_rela)) {
            print("elf: Unknown sh_entsize for RELA section!\n");
            return 1;
        }

        for (uint16_t j = 0; j < hdr->ph_num; j++) {
            struct elf64_phdr _phdr;
            memcpy(&_phdr, elf + (hdr->phoff + j * hdr->phdr_size),
                   sizeof(struct elf64_phdr));

            if (_phdr.p_vaddr <= rela_offset && _phdr.p_vaddr + _phdr.p_filesz > rela_offset) {
                rela_offset -= _phdr.p_vaddr;
                rela_offset += _phdr.p_offset;
                break;
            }
        }

        // This is a RELA header, get and apply all relocations
        for (uint64_t offset = 0; offset < rela_size; offset += rela_ent) {
            struct elf64_rela relocation;
            memcpy(&relocation, elf + (rela_offset + offset), sizeof(struct elf64_rela));

            switch (relocation.r_info) {
                case R_X86_64_RELATIVE: {
                    // Relocation is before buffer
                    if (relocation.r_addr < vaddr)
                        continue;

                    // Relocation is after buffer
                    if (vaddr + size < relocation.r_addr + 8)
                        continue;

                    // It's inside it, calculate where it is
                    uint64_t *ptr = (uint64_t *)((uint8_t *)buffer - vaddr + relocation.r_addr);

                    // Write the relocated value
                    *ptr = slide + relocation.r_addend;
                    break;
                }
                default:
                    print("elf: Unknown RELA type: %x\n", relocation.r_info);
                    return 1;
            }
        }

        break;
    }

    return 0;
}

int elf64_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit, uint64_t slide) {
    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf64_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return 1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        printv("elf: Not a Little-endian ELF file.\n");
        return 1;
    }

    if (hdr.machine != ARCH_X86_64) {
        printv("elf: Not an x86_64 ELF file.\n");
        return 1;
    }

    if (hdr.shdr_size < sizeof(struct elf64_shdr)) {
        panic(true, "elf: shdr_size < sizeof(struct elf64_shdr)");
    }

    struct elf64_shdr shstrtab;
    memcpy(&shstrtab, elf + (hdr.shoff + hdr.shstrndx * hdr.shdr_size),
           sizeof(struct elf64_shdr));

    char *names = ext_mem_alloc(shstrtab.sh_size);
    memcpy(names, elf + (shstrtab.sh_offset), shstrtab.sh_size);

    int ret;

    for (uint16_t i = 0; i < hdr.sh_num; i++) {
        struct elf64_shdr section;
        memcpy(&section, elf + (hdr.shoff + i * hdr.shdr_size),
               sizeof(struct elf64_shdr));

        if (!strcmp(&names[section.sh_name], name)) {
            if (limit == 0) {
                *(void **)buffer = ext_mem_alloc(section.sh_size);
                buffer = *(void **)buffer;
                limit = section.sh_size;
            }
            if (section.sh_size > limit) {
                ret = 3;
                goto out;
            }
            if (section.sh_size < limit) {
                ret = 4;
                goto out;
            }
            memcpy(buffer, elf + (section.sh_offset), section.sh_size);
            ret = elf64_apply_relocations(elf, &hdr, buffer, section.sh_addr, section.sh_size, slide);
            goto out;
        }
    }

    ret = 2;

out:
    pmm_free(names, shstrtab.sh_size);

    return ret;
}

/// SAFETY: The caller must ensure that the provided `elf` is a valid 64-bit
/// ELF file.
struct elf_section_hdr_info* elf64_section_hdr_info(uint8_t *elf) {
    struct elf_section_hdr_info* info = ext_mem_alloc(sizeof(struct elf_section_hdr_info));

    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf64_hdr));

    info->num = hdr.sh_num;
    info->section_entry_size = hdr.shdr_size;
    info->section_hdr_size = info->num * info->section_entry_size;
    info->str_section_idx = hdr.shstrndx;
    info->section_hdrs = ext_mem_alloc(info->section_hdr_size);

    memcpy(info->section_hdrs, elf + (hdr.shoff), info->section_hdr_size);

    return info;
}

/// SAFETY: The caller must ensure that the provided `elf` is a valid 32-bit
/// ELF file.
struct elf_section_hdr_info* elf32_section_hdr_info(uint8_t *elf) {
    struct elf_section_hdr_info* info = ext_mem_alloc(sizeof(struct elf_section_hdr_info));

    struct elf32_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf32_hdr));

    info->num = hdr.sh_num;
    info->section_entry_size = hdr.shdr_size;
    info->section_hdr_size = info->num * info->section_entry_size;
    info->str_section_idx = hdr.shstrndx;
    info->section_hdrs = ext_mem_alloc(info->section_hdr_size);

    memcpy(info->section_hdrs, elf + (hdr.shoff), info->section_hdr_size);

    return info;
}

int elf32_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit) {
    struct elf32_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf32_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return 1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        printv("elf: Not a Little-endian ELF file.\n");
        return 1;
    }

    if (hdr.machine != ARCH_X86_32) {
        printv("elf: Not an x86_32 ELF file.\n");
        return 1;
    }

    if (hdr.shdr_size < sizeof(struct elf32_shdr)) {
        panic(true, "elf: shdr_size < sizeof(struct elf32_shdr)");
    }

    struct elf32_shdr shstrtab;
    memcpy(&shstrtab, elf + (hdr.shoff + hdr.shstrndx * hdr.shdr_size),
           sizeof(struct elf32_shdr));

    char *names = ext_mem_alloc(shstrtab.sh_size);
    memcpy(names, elf + (shstrtab.sh_offset), shstrtab.sh_size);

    int ret;

    for (uint16_t i = 0; i < hdr.sh_num; i++) {
        struct elf32_shdr section;
        memcpy(&section, elf + (hdr.shoff + i * hdr.shdr_size),
               sizeof(struct elf32_shdr));

        if (!strcmp(&names[section.sh_name], name)) {
            if (section.sh_size > limit) {
                ret = 3;
                goto out;
            }
            if (section.sh_size < limit) {
                ret = 4;
                goto out;
            }
            memcpy(buffer, elf + (section.sh_offset), section.sh_size);
            ret = 0;
            goto out;
        }
    }

    ret = 2;

out:
    pmm_free(names, shstrtab.sh_size);

    return ret;
}

static uint64_t elf64_max_align(uint8_t *elf) {
    uint64_t ret = 0;

    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf64_hdr));

    if (hdr.phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf64_phdr phdr;
        memcpy(&phdr, elf + (hdr.phoff + i * hdr.phdr_size),
               sizeof(struct elf64_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        if (phdr.p_align > ret) {
            ret = phdr.p_align;
        }
    }

    if (ret == 0) {
        panic(true, "elf: Executable has no loadable segments");
    }

    return ret;
}

static void elf64_get_ranges(uint8_t *elf, uint64_t slide, bool use_paddr, struct elf_range **_ranges, uint64_t *_ranges_count) {
    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf64_hdr));

    uint64_t ranges_count = 0;

    if (hdr.phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf64_phdr phdr;
        memcpy(&phdr, elf + (hdr.phoff + i * hdr.phdr_size),
               sizeof(struct elf64_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        if (!use_paddr && phdr.p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
            continue;
        }

        ranges_count++;
    }

    if (ranges_count == 0) {
        panic(true, "elf: Attempted to use PMRs but no higher half PHDRs exist");
    }

    struct elf_range *ranges = ext_mem_alloc(ranges_count * sizeof(struct elf_range));

    size_t r = 0;
    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf64_phdr phdr;
        memcpy(&phdr, elf + (hdr.phoff + i * hdr.phdr_size),
               sizeof(struct elf64_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        uint64_t load_addr = 0;

        if (use_paddr) {
            load_addr = phdr.p_paddr;
        } else {
            load_addr = phdr.p_vaddr;

            if (phdr.p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
                continue;
            }
        }

        load_addr += slide;

        uint64_t this_top = load_addr + phdr.p_memsz;

        ranges[r].base = load_addr & ~(phdr.p_align - 1);
        ranges[r].length = ALIGN_UP(this_top - ranges[r].base, phdr.p_align);
        ranges[r].permissions = phdr.p_flags & 0b111;

        r++;
    }

    *_ranges_count = ranges_count;
    *_ranges = ranges;
}

int elf64_load(uint8_t *elf, uint64_t *entry_point, uint64_t *top, uint64_t *_slide, uint32_t alloc_type, bool kaslr, bool use_paddr, struct elf_range **ranges, uint64_t *ranges_count, bool fully_virtual, uint64_t *physical_base, uint64_t *virtual_base, uint64_t *_image_size, bool *is_reloc) {
    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf64_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return -1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        panic(true, "elf: Not a Little-endian ELF file.\n");
    }

    if (hdr.machine != ARCH_X86_64) {
        panic(true, "elf: Not an x86_64 ELF file.\n");
    }

    if (is_reloc) {
        *is_reloc = false;
    }

    uint64_t slide = 0;
    bool simulation = true;
    size_t try_count = 0;
    size_t max_simulated_tries = 0x100000;

    uint64_t entry = hdr.entry;
    bool entry_adjusted = false;

    uint64_t max_align = elf64_max_align(elf);

    uint64_t image_size = 0;

    if (hdr.phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    if (fully_virtual) {
        simulation = false;

        uint64_t min_vaddr = (uint64_t)-1;
        uint64_t max_vaddr = 0;
        for (uint16_t i = 0; i < hdr.ph_num; i++) {
            struct elf64_phdr phdr;
            memcpy(&phdr, elf + (hdr.phoff + i * hdr.phdr_size),
                   sizeof(struct elf64_phdr));

            if (phdr.p_type != PT_LOAD) {
                continue;
            }

            // Drop entries not in the higher half
            if (phdr.p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
                continue;
            }

            if (phdr.p_vaddr < min_vaddr) {
                min_vaddr = phdr.p_vaddr;
            }

            if (phdr.p_vaddr + phdr.p_memsz > max_vaddr) {
                max_vaddr = phdr.p_vaddr + phdr.p_memsz;
            }
        }

        if (max_vaddr == 0 || min_vaddr == (uint64_t)-1) {
            panic(true, "elf: Attempted to use fully virtual mappings but no higher half PHDRs exist");
        }

        image_size = max_vaddr - min_vaddr;

        *physical_base = (uintptr_t)ext_mem_alloc_type_aligned(image_size, alloc_type, max_align);
        *virtual_base = min_vaddr;
        if (_image_size) {
            *_image_size = image_size;
        }
    }

    if (!elf64_is_relocatable(elf, &hdr)) {
        simulation = false;
        goto final;
    } else {
        if (is_reloc) {
            *is_reloc = true;
        }
    }

again:
    if (kaslr) {
        slide = rand32() & ~(max_align - 1);

        if (fully_virtual) {
            if ((*virtual_base - FIXED_HIGHER_HALF_OFFSET_64) + slide + image_size >= 0x80000000) {
                if (++try_count == max_simulated_tries) {
                    panic(true, "elf: Image wants to load too high");
                }
                goto again;
            }
        }
    }

final:
    if (top)
        *top = 0;

    bool higher_half = false;

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf64_phdr phdr;
        memcpy(&phdr, elf + (hdr.phoff + i * hdr.phdr_size),
               sizeof(struct elf64_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        // Sanity checks
        if (phdr.p_filesz > phdr.p_memsz) {
            panic(true, "elf: p_filesz > p_memsz");
        }

        uint64_t load_addr = 0;

        if (use_paddr) {
            load_addr = phdr.p_paddr;
        } else {
            load_addr = phdr.p_vaddr;

            if (phdr.p_vaddr >= FIXED_HIGHER_HALF_OFFSET_64) {
                higher_half = true;

                if (fully_virtual) {
                    load_addr = *physical_base + (phdr.p_vaddr - *virtual_base);
                } else {
                    load_addr = phdr.p_vaddr - FIXED_HIGHER_HALF_OFFSET_64;
                }
            } else if (ranges) {
                // Drop lower half
                continue;
            }
        }

        if (!fully_virtual) {
            load_addr += slide;
        }

        uint64_t this_top = load_addr + phdr.p_memsz;

        if (top) {
            if (this_top > *top) {
                *top = this_top;
            }
        }

        uint64_t mem_base, mem_size;

        if (ranges) {
            mem_base = load_addr & ~(phdr.p_align - 1);
            mem_size = this_top - mem_base;
        } else {
            mem_base = load_addr;
            mem_size = phdr.p_memsz;
        }

        if (!fully_virtual &&
            ((higher_half == true && this_top > 0x80000000)
          || !memmap_alloc_range((size_t)mem_base, (size_t)mem_size, alloc_type, true, false, simulation, false))) {
            if (simulation == false || ++try_count == max_simulated_tries) {
                panic(true, "elf: Failed to allocate necessary memory range (%X-%X)", mem_base, mem_base + mem_size);
            }
            if (!kaslr) {
                slide += max_align;
            }
            goto again;
        }

        if (simulation) {
            continue;
        }

        memcpy((void *)(uintptr_t)load_addr, elf + (phdr.p_offset), phdr.p_filesz);

        size_t to_zero = (size_t)(phdr.p_memsz - phdr.p_filesz);

        if (to_zero) {
            void *ptr = (void *)(uintptr_t)(load_addr + phdr.p_filesz);
            memset(ptr, 0, to_zero);
        }

        if (elf64_apply_relocations(elf, &hdr, (void *)(uintptr_t)load_addr, phdr.p_vaddr, phdr.p_memsz, slide)) {
            panic(true, "elf: Failed to apply relocations");
        }

        if (use_paddr) {
            if (!entry_adjusted && entry >= phdr.p_vaddr && entry < (phdr.p_vaddr + phdr.p_memsz)) {
                entry -= phdr.p_vaddr;
                entry += phdr.p_paddr;
                entry_adjusted = true;
            }
        }
    }

    if (simulation) {
        simulation = false;
        goto final;
    }

    if (fully_virtual) {
        *virtual_base += slide;
    }

    *entry_point = entry + slide;
    if (_slide)
        *_slide = slide;

    if (ranges_count != NULL && ranges != NULL) {
        elf64_get_ranges(elf, slide, use_paddr, ranges, ranges_count);
    }

    return 0;
}

int elf32_load(uint8_t *elf, uint32_t *entry_point, uint32_t *top, uint32_t alloc_type) {
    struct elf32_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf32_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return -1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        printv("elf: Not a Little-endian ELF file.\n");
        return -1;
    }

    if (hdr.machine != ARCH_X86_32) {
        printv("elf: Not an x86_32 ELF file.\n");
        return -1;
    }

    uint32_t entry = hdr.entry;
    bool entry_adjusted = false;

    if (top)
        *top = 0;

    if (hdr.phdr_size < sizeof(struct elf32_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf32_phdr)");
    }

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf32_phdr phdr;
        memcpy(&phdr, elf + (hdr.phoff + i * hdr.phdr_size),
               sizeof(struct elf32_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        // Sanity checks
        if (phdr.p_filesz > phdr.p_memsz) {
            panic(true, "elf: p_filesz > p_memsz");
        }

        if (top) {
            uint32_t this_top = phdr.p_paddr + phdr.p_memsz;
            if (this_top > *top) {
                *top = this_top;
            }
        }

        memmap_alloc_range((size_t)phdr.p_paddr, (size_t)phdr.p_memsz, alloc_type, true, true, false, false);

        memcpy((void *)(uintptr_t)phdr.p_paddr, elf + (phdr.p_offset), phdr.p_filesz);

        size_t to_zero = (size_t)(phdr.p_memsz - phdr.p_filesz);

        if (to_zero) {
            void *ptr = (void *)(uintptr_t)(phdr.p_paddr + phdr.p_filesz);
            memset(ptr, 0, to_zero);
        }

        if (!entry_adjusted && entry >= phdr.p_vaddr && entry < (phdr.p_vaddr + phdr.p_memsz)) {
            entry -= phdr.p_vaddr;
            entry += phdr.p_paddr;
            entry_adjusted = true;
        }
    }

    *entry_point = entry;

    return 0;
}
