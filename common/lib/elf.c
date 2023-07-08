#include <stdint.h>
#include <stddef.h>
#include <lib/misc.h>
#include <sys/cpu.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <lib/print.h>
#include <lib/rand.h>
#include <lib/elsewhere.h>
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
#define DT_FLAGS_1  0x6ffffffb

#define DF_1_PIE    0x08000000

#define ABI_SYSV     0x00
#define ARCH_X86_64  0x3e
#define ARCH_X86_32  0x03
#define ARCH_AARCH64 0xb7
#define BITS_LE      0x01
#define ET_DYN       0x0003
#define SHT_RELA     0x00000004
#define R_X86_64_RELATIVE  0x00000008
#define R_AARCH64_RELATIVE 0x00000403

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
    struct elf64_hdr *hdr = (void *)elf;

    if (strncmp((char *)hdr->ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return -1;
    }

    switch (hdr->machine) {
        case ARCH_X86_64:
        case ARCH_AARCH64:
            return 64;
        case ARCH_X86_32:
            return 32;
        default:
            return -1;
    }
}

struct elf_section_hdr_info elf64_section_hdr_info(uint8_t *elf) {
    struct elf_section_hdr_info info = {0};

    struct elf64_hdr *hdr = (void *)elf;

    info.num = hdr->sh_num;
    info.section_entry_size = hdr->shdr_size;
    info.str_section_idx = hdr->shstrndx;
    info.section_offset = hdr->shoff;

    return info;
}

struct elf_section_hdr_info elf32_section_hdr_info(uint8_t *elf) {
    struct elf_section_hdr_info info = {0};

    struct elf32_hdr *hdr = (void *)elf;

    info.num = hdr->sh_num;
    info.section_entry_size = hdr->shdr_size;
    info.str_section_idx = hdr->shstrndx;
    info.section_offset = hdr->shoff;

    return info;
}

static bool elf64_is_relocatable(uint8_t *elf, struct elf64_hdr *hdr) {
    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    // Find DYN segment
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_DYNAMIC) {
            continue;
        }

        for (uint16_t j = 0; j < phdr->p_filesz / sizeof(struct elf64_dyn); j++) {
            struct elf64_dyn *dyn = (void *)elf + (phdr->p_offset + j * sizeof(struct elf64_dyn));

            switch (dyn->d_tag) {
                case DT_FLAGS_1:
                    if (dyn->d_un & DF_1_PIE) {
                        return true;
                    }
                    break;
                case DT_RELA:
                    return true;
            }
        }
    }

    return false;
}

static bool elf64_apply_relocations(uint8_t *elf, struct elf64_hdr *hdr, void *buffer, uint64_t vaddr, size_t size, uint64_t slide) {
    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    // Find DYN segment
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_DYNAMIC)
            continue;

        uint64_t rela_offset = 0;
        uint64_t rela_size = 0;
        uint64_t rela_ent = 0;
        for (uint16_t j = 0; j < phdr->p_filesz / sizeof(struct elf64_dyn); j++) {
            struct elf64_dyn *dyn = (void *)elf + (phdr->p_offset + j * sizeof(struct elf64_dyn));

            switch (dyn->d_tag) {
                case DT_RELA:
                    rela_offset = dyn->d_un;
                    break;
                case DT_RELAENT:
                    rela_ent = dyn->d_un;
                    break;
                case DT_RELASZ:
                    rela_size = dyn->d_un;
                    break;
            }
        }

        if (rela_offset == 0) {
            break;
        }

        if (rela_ent != sizeof(struct elf64_rela)) {
            print("elf: Unknown sh_entsize for RELA section!\n");
            return false;
        }

        for (uint16_t j = 0; j < hdr->ph_num; j++) {
            struct elf64_phdr *_phdr = (void *)elf + (hdr->phoff + j * hdr->phdr_size);

            if (_phdr->p_vaddr <= rela_offset && _phdr->p_vaddr + _phdr->p_filesz > rela_offset) {
                rela_offset -= _phdr->p_vaddr;
                rela_offset += _phdr->p_offset;
                break;
            }
        }

        // This is a RELA header, get and apply all relocations
        for (uint64_t offset = 0; offset < rela_size; offset += rela_ent) {
            struct elf64_rela *relocation = (void *)elf + (rela_offset + offset);

            switch (relocation->r_info) {
#if defined (__x86_64__) || defined (__i386__)
                case R_X86_64_RELATIVE:
#elif defined (__aarch64__)
                case R_AARCH64_RELATIVE:
#else
#error Unknown architecture
#endif
                {
                    // Relocation is before buffer
                    if (relocation->r_addr < vaddr)
                        continue;

                    // Relocation is after buffer
                    if (vaddr + size < relocation->r_addr + 8)
                        continue;

                    // It's inside it, calculate where it is
                    uint64_t *ptr = (uint64_t *)((uint8_t *)buffer - vaddr + relocation->r_addr);

                    // Write the relocated value
                    *ptr = slide + relocation->r_addend;
                    break;
                }
                default: {
                    print("elf: Unknown RELA type: %x\n", relocation->r_info);
                    return false;
                }
            }
        }

        break;
    }

    return true;
}

bool elf64_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit, uint64_t slide) {
    struct elf64_hdr *hdr = (void *)elf;

    if (strncmp((char *)hdr->ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return false;
    }

    if (hdr->ident[EI_DATA] != BITS_LE) {
        printv("elf: Not a Little-endian ELF file.\n");
        return false;
    }

#if defined (__x86_64__) || defined (__i386__)
    if (hdr->machine != ARCH_X86_64) {
        printv("elf: Not an x86_64 ELF file.\n");
        return false;
    }
#elif defined (__aarch64__)
    if (hdr->machine != ARCH_AARCH64) {
        printv("elf: Not an aarch64 ELF file.\n");
        return false;
    }
#else
#error Unknown architecture
#endif

    if (hdr->shdr_size < sizeof(struct elf64_shdr)) {
        panic(true, "elf: shdr_size < sizeof(struct elf64_shdr)");
    }

    struct elf64_shdr *shstrtab = (void *)elf + (hdr->shoff + hdr->shstrndx * hdr->shdr_size);

    char *names = (void *)elf + shstrtab->sh_offset;

    for (uint16_t i = 0; i < hdr->sh_num; i++) {
        struct elf64_shdr *section = (void *)elf + (hdr->shoff + i * hdr->shdr_size);

        if (strcmp(&names[section->sh_name], name) == 0) {
            if (limit == 0) {
                *(void **)buffer = ext_mem_alloc(section->sh_size);
                buffer = *(void **)buffer;
                limit = section->sh_size;
            }
            if (section->sh_size > limit) {
                return false;
            }
            memcpy(buffer, elf + section->sh_offset, section->sh_size);
            return elf64_apply_relocations(elf, hdr, buffer, section->sh_addr, section->sh_size, slide);
        }
    }

    return false;
}

static uint64_t elf64_max_align(uint8_t *elf) {
    uint64_t ret = 0;

    struct elf64_hdr *hdr = (void *)elf;

    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_align > ret) {
            ret = phdr->p_align;
        }
    }

    if (ret == 0) {
        panic(true, "elf: Executable has no loadable segments");
    }

    return ret;
}

static void elf64_get_ranges(uint8_t *elf, uint64_t slide, struct elf_range **_ranges, uint64_t *_ranges_count) {
    struct elf64_hdr *hdr = (void *)elf;

    uint64_t ranges_count = 0;

    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
            continue;
        }

        ranges_count++;
    }

    if (ranges_count == 0) {
        panic(true, "elf: No higher half PHDRs exist");
    }

    struct elf_range *ranges = ext_mem_alloc(ranges_count * sizeof(struct elf_range));

    size_t r = 0;
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (phdr->p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
            continue;
        }

        uint64_t load_addr = phdr->p_vaddr + slide;
        uint64_t this_top = load_addr + phdr->p_memsz;

        ranges[r].base = load_addr & ~(phdr->p_align - 1);
        ranges[r].length = ALIGN_UP(this_top - ranges[r].base, phdr->p_align);
        ranges[r].permissions = phdr->p_flags & 0b111;

        r++;
    }

    *_ranges_count = ranges_count;
    *_ranges = ranges;
}

bool elf64_load(uint8_t *elf, uint64_t *entry_point, uint64_t *_slide, uint32_t alloc_type, bool kaslr, struct elf_range **ranges, uint64_t *ranges_count, uint64_t *physical_base, uint64_t *virtual_base, uint64_t *_image_size, uint64_t *_image_size_before_bss, bool *is_reloc) {
    struct elf64_hdr *hdr = (void *)elf;

    if (strncmp((char *)hdr->ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return false;
    }

    if (hdr->ident[EI_DATA] != BITS_LE) {
        panic(true, "elf: Not a Little-endian ELF file.\n");
    }

#if defined (__x86_64__) || defined (__i386__)
    if (hdr->machine != ARCH_X86_64) {
        panic(true, "elf: Not an x86_64 ELF file.\n");
    }
#elif defined (__aarch64__)
    if (hdr->machine != ARCH_AARCH64) {
        panic(true, "elf: Not an aarch64 ELF file.\n");
    }
#else
#error Unknown architecture
#endif

    if (is_reloc) {
        *is_reloc = false;
    }

    uint64_t slide = 0;
    size_t try_count = 0;
    size_t max_simulated_tries = 0x10000;

    uint64_t entry = hdr->entry;

    uint64_t max_align = elf64_max_align(elf);

    uint64_t image_size = 0;

    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    uint64_t min_vaddr = (uint64_t)-1;
    uint64_t max_vaddr = 0;
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        // Drop entries not in the higher half
        if (phdr->p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
            continue;
        }

        // check for overlapping phdrs
        for (uint16_t j = 0; j < hdr->ph_num; j++) {
            struct elf64_phdr *phdr_in = (void *)elf + (hdr->phoff + j * hdr->phdr_size);

            if (phdr_in->p_type != PT_LOAD) {
                continue;
            }

            // Drop entries not in the higher half
            if (phdr_in->p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
                continue;
            }

            if (phdr_in == phdr) {
                continue;
            }

            if ((phdr_in->p_vaddr >= phdr->p_vaddr
              && phdr_in->p_vaddr < phdr->p_vaddr + phdr->p_memsz)
                ||
                (phdr_in->p_vaddr + phdr_in->p_memsz > phdr->p_vaddr
              && phdr_in->p_vaddr + phdr_in->p_memsz <= phdr->p_vaddr + phdr->p_memsz)) {
                panic(true, "elf: Attempted to load ELF file with overlapping PHDRs (%u and %u overlap)", i, j);
            }
        }

        if (phdr->p_vaddr < min_vaddr) {
            min_vaddr = phdr->p_vaddr;
        }

        if (phdr->p_vaddr + phdr->p_memsz > max_vaddr) {
            max_vaddr = phdr->p_vaddr + phdr->p_memsz;
        }
    }

    if (max_vaddr == 0 || min_vaddr == (uint64_t)-1) {
        panic(true, "elf: No higher half PHDRs exist");
    }

    image_size = max_vaddr - min_vaddr;

    *physical_base = (uintptr_t)ext_mem_alloc_type_aligned(image_size, alloc_type, max_align);
    *virtual_base = min_vaddr;

    if (_image_size) {
        *_image_size = image_size;
    }

    if (elf64_is_relocatable(elf, hdr)) {
        if (is_reloc) {
            *is_reloc = true;
        }
    }

again:
    if (*is_reloc && kaslr) {
        slide = rand32() & ~(max_align - 1);

        if ((*virtual_base - FIXED_HIGHER_HALF_OFFSET_64) + slide + image_size >= 0x80000000) {
            if (++try_count == max_simulated_tries) {
                panic(true, "elf: Image wants to load too high");
            }
            goto again;
        }
    }

    uint64_t bss_size;

    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        // Drop entries not in the higher half
        if (phdr->p_vaddr < FIXED_HIGHER_HALF_OFFSET_64) {
            continue;
        }

        // Sanity checks
        if (phdr->p_filesz > phdr->p_memsz) {
            panic(true, "elf: p_filesz > p_memsz");
        }

        uint64_t load_addr = *physical_base + (phdr->p_vaddr - *virtual_base);

#if defined (__aarch64__)
        uint64_t this_top = load_addr + phdr->p_memsz;

        uint64_t mem_base, mem_size;

        mem_base = load_addr & ~(phdr->p_align - 1);
        mem_size = this_top - mem_base;
#endif

        memcpy((void *)(uintptr_t)load_addr, elf + (phdr->p_offset), phdr->p_filesz);

        if (i == hdr->ph_num - 1) {
            bss_size = phdr->p_memsz - phdr->p_filesz;
        }

        if (!elf64_apply_relocations(elf, hdr, (void *)(uintptr_t)load_addr, phdr->p_vaddr, phdr->p_memsz, slide)) {
            panic(true, "elf: Failed to apply relocations");
        }

#if defined (__aarch64__)
        clean_inval_dcache_poc(mem_base, mem_base + mem_size);
        inval_icache_pou(mem_base, mem_base + mem_size);
#endif
    }

    if (_image_size_before_bss != NULL) {
        *_image_size_before_bss = image_size - bss_size;
    }

    *virtual_base += slide;
    *entry_point = entry + slide;
    if (_slide) {
        *_slide = slide;
    }

    if (ranges_count != NULL && ranges != NULL) {
        elf64_get_ranges(elf, slide, ranges, ranges_count);
    }

    return true;
}

bool elf32_load_elsewhere(uint8_t *elf, uint64_t *entry_point,
                          struct elsewhere_range **ranges,
                          uint64_t *ranges_count) {
    struct elf32_hdr *hdr = (void *)elf;

    if (strncmp((char *)hdr->ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return false;
    }

    if (hdr->ident[EI_DATA] != BITS_LE) {
        printv("elf: Not a Little-endian ELF file.\n");
        return false;
    }

    if (hdr->machine != ARCH_X86_32) {
        printv("elf: Not an x86_32 ELF file.\n");
        return false;
    }

    *entry_point = hdr->entry;
    bool entry_adjusted = false;

    if (hdr->phdr_size < sizeof(struct elf32_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf32_phdr)");
    }

    *ranges_count = 0;
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf32_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD)
            continue;

        *ranges_count += 1;
    }

    *ranges = ext_mem_alloc(sizeof(struct elsewhere_range) * *ranges_count);

    size_t cur_entry = 0;

    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf32_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD)
            continue;

        // Sanity checks
        if (phdr->p_filesz > phdr->p_memsz) {
            panic(true, "elf: p_filesz > p_memsz");
        }

        void *elsewhere = ext_mem_alloc(phdr->p_memsz);

        memcpy(elsewhere, elf + phdr->p_offset, phdr->p_filesz);

        if (!entry_adjusted
         && *entry_point >= phdr->p_vaddr
         && *entry_point < (phdr->p_vaddr + phdr->p_memsz)) {
            *entry_point -= phdr->p_vaddr;
            *entry_point += phdr->p_paddr;
            entry_adjusted = true;
        }

        (*ranges)[cur_entry].elsewhere = (uintptr_t)elsewhere;
        (*ranges)[cur_entry].target = phdr->p_paddr;
        (*ranges)[cur_entry].length = phdr->p_memsz;

        cur_entry++;
    }

    return true;
}

bool elf64_load_elsewhere(uint8_t *elf, uint64_t *entry_point,
                          struct elsewhere_range **ranges,
                          uint64_t *ranges_count) {
    struct elf64_hdr *hdr = (void *)elf;

    if (strncmp((char *)hdr->ident, "\177ELF", 4)) {
        printv("elf: Not a valid ELF file.\n");
        return false;
    }

    if (hdr->ident[EI_DATA] != BITS_LE) {
        printv("elf: Not a Little-endian ELF file.\n");
        return false;
    }

    if (hdr->machine != ARCH_X86_64) {
        printv("elf: Not an x86_64 ELF file.\n");
        return false;
    }

    *entry_point = hdr->entry;
    bool entry_adjusted = false;

    if (hdr->phdr_size < sizeof(struct elf64_phdr)) {
        panic(true, "elf: phdr_size < sizeof(struct elf64_phdr)");
    }

    *ranges_count = 0;
    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD)
            continue;

        *ranges_count += 1;
    }

    *ranges = ext_mem_alloc(sizeof(struct elsewhere_range) * *ranges_count);

    size_t cur_entry = 0;

    for (uint16_t i = 0; i < hdr->ph_num; i++) {
        struct elf64_phdr *phdr = (void *)elf + (hdr->phoff + i * hdr->phdr_size);

        if (phdr->p_type != PT_LOAD)
            continue;

        // Sanity checks
        if (phdr->p_filesz > phdr->p_memsz) {
            panic(true, "elf: p_filesz > p_memsz");
        }

        void *elsewhere = ext_mem_alloc(phdr->p_memsz);

        memcpy(elsewhere, elf + phdr->p_offset, phdr->p_filesz);

        if (!entry_adjusted
         && *entry_point >= phdr->p_vaddr
         && *entry_point < (phdr->p_vaddr + phdr->p_memsz)) {
            *entry_point -= phdr->p_vaddr;
            *entry_point += phdr->p_paddr;
            entry_adjusted = true;
        }

        (*ranges)[cur_entry].elsewhere = (uintptr_t)elsewhere;
        (*ranges)[cur_entry].target = phdr->p_paddr;
        (*ranges)[cur_entry].length = phdr->p_memsz;

        cur_entry++;
    }

    return true;
}
