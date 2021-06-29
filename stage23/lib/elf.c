#include <stdint.h>
#include <stddef.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <lib/print.h>
#include <lib/rand.h>
#include <mm/pmm.h>
#include <fs/file.h>

#define KASLR_SLIDE_BITMASK ((uintptr_t)0x3ffff000)

#define PT_LOAD     0x00000001
#define PT_INTERP   0x00000003
#define PT_PHDR     0x00000006

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

struct elf64_hdr {
    uint8_t  ident[16];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t hdr_size;
    uint16_t phdr_size;
    uint16_t ph_num;
    uint16_t shdr_size;
    uint16_t sh_num;
    uint16_t shstrndx;
};

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

struct elf64_shdr {
    uint32_t sh_name;
    uint32_t sh_type;
    uint64_t sh_flags;
    uint64_t sh_addr;
    uint64_t sh_offset;
    uint64_t sh_size;
    uint32_t sh_link;
    uint32_t sh_info;
    uint64_t sh_addralign;
    uint64_t sh_entsize;
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

int elf_bits(uint8_t *elf) {
    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), 20);

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        print("elf: Not a valid ELF file.\n");
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
    // Find RELA sections
    for (uint16_t i = 0; i < hdr->sh_num; i++) {
        struct elf64_shdr section;
        memcpy(&section, elf + (hdr->shoff + i * sizeof(struct elf64_shdr)),
                    sizeof(struct elf64_shdr));

        if (section.sh_type != SHT_RELA)
            continue;

        if (section.sh_entsize != sizeof(struct elf64_rela)) {
            print("elf: Unknown sh_entsize for RELA section!\n");
            continue;
        }

        return true;
    }

    return false;
}

static int elf64_apply_relocations(uint8_t *elf, struct elf64_hdr *hdr, void *buffer, uint64_t vaddr, size_t size, uint64_t slide) {
    // Find RELA sections
    for (uint16_t i = 0; i < hdr->sh_num; i++) {
        struct elf64_shdr section;
        memcpy(&section, elf + (hdr->shoff + i * sizeof(struct elf64_shdr)),
                    sizeof(struct elf64_shdr));

        if (section.sh_type != SHT_RELA)
            continue;

        if (section.sh_entsize != sizeof(struct elf64_rela)) {
            print("elf: Unknown sh_entsize for RELA section!\n");
            return 1;
        }

        // This is a RELA header, get and apply all relocations
        for (uint64_t offset = 0; offset < section.sh_size; offset += section.sh_entsize) {
            struct elf64_rela relocation;
            memcpy(&relocation, elf + (section.sh_offset + offset), sizeof(relocation));

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
    }

    return 0;
}

int elf64_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit, uint64_t slide) {
    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf64_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        print("elf: Not a valid ELF file.\n");
        return 1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        print("elf: Not a Little-endian ELF file.\n");
        return 1;
    }

    if (hdr.machine != ARCH_X86_64) {
        print("elf: Not an x86_64 ELF file.\n");
        return 1;
    }

    struct elf64_shdr shstrtab;
    memcpy(&shstrtab, elf + (hdr.shoff + hdr.shstrndx * sizeof(struct elf64_shdr)),
            sizeof(struct elf64_shdr));

    char *names = ext_mem_alloc(shstrtab.sh_size);
    memcpy(names, elf + (shstrtab.sh_offset), shstrtab.sh_size);

    for (uint16_t i = 0; i < hdr.sh_num; i++) {
        struct elf64_shdr section;
        memcpy(&section, elf + (hdr.shoff + i * sizeof(struct elf64_shdr)),
                   sizeof(struct elf64_shdr));

        if (!strcmp(&names[section.sh_name], name)) {
            if (section.sh_size > limit)
                return 3;
            if (section.sh_size < limit)
                return 4;
            memcpy(buffer, elf + (section.sh_offset), section.sh_size);
            return elf64_apply_relocations(elf, &hdr, buffer, section.sh_addr, section.sh_size, slide);
        }
    }

    return 2;
}

int elf32_load_section(uint8_t *elf, void *buffer, const char *name, size_t limit) {
    struct elf32_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf32_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        print("elf: Not a valid ELF file.\n");
        return 1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        print("elf: Not a Little-endian ELF file.\n");
        return 1;
    }

    if (hdr.machine != ARCH_X86_32) {
        print("elf: Not an x86_32 ELF file.\n");
        return 1;
    }

    struct elf32_shdr shstrtab;
    memcpy(&shstrtab, elf + (hdr.shoff + hdr.shstrndx * sizeof(struct elf32_shdr)),
            sizeof(struct elf32_shdr));

    char *names = ext_mem_alloc(shstrtab.sh_size);
    memcpy(names, elf + (shstrtab.sh_offset), shstrtab.sh_size);

    for (uint16_t i = 0; i < hdr.sh_num; i++) {
        struct elf32_shdr section;
        memcpy(&section, elf + (hdr.shoff + i * sizeof(struct elf32_shdr)),
                   sizeof(struct elf32_shdr));

        if (!strcmp(&names[section.sh_name], name)) {
            if (section.sh_size > limit)
                return 3;
            if (section.sh_size < limit)
                return 4;
            memcpy(buffer, elf + (section.sh_offset), section.sh_size);
            return 0;
        }
    }

    return 2;
}

int elf64_load(uint8_t *elf, uint64_t *entry_point, uint64_t *_slide, uint32_t alloc_type, bool kaslr) {
    struct elf64_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf64_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        print("elf: Not a valid ELF file.\n");
        return -1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        print("elf: Not a Little-endian ELF file.\n");
        return -1;
    }

    if (hdr.machine != ARCH_X86_64) {
        print("elf: Not an x86_64 ELF file.\n");
        return -1;
    }

    uint64_t slide = 0;
    bool simulation = true;
    size_t try_count = 0;
    size_t max_simulated_tries = 250;

    if (!elf64_is_relocatable(elf, &hdr)) {
        simulation = false;
        goto final;
    }

again:
    if (kaslr)
        slide = rand64() & KASLR_SLIDE_BITMASK;

final:
    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf64_phdr phdr;
        memcpy(&phdr, elf + (hdr.phoff + i * sizeof(struct elf64_phdr)),
                   sizeof(struct elf64_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        uint64_t load_vaddr = phdr.p_vaddr;

        if (load_vaddr & ((uint64_t)1 << 63))
            load_vaddr -= FIXED_HIGHER_HALF_OFFSET_64;

        load_vaddr += slide;

        if (!memmap_alloc_range((size_t)load_vaddr, (size_t)phdr.p_memsz, alloc_type, true, false, simulation, false)) {
            if (++try_count == max_simulated_tries || simulation == false)
                return -1;
            if (!kaslr)
                slide += 0x1000;
            goto again;
        }

        memcpy((void *)(uintptr_t)load_vaddr, elf + (phdr.p_offset), phdr.p_filesz);

        size_t to_zero = (size_t)(phdr.p_memsz - phdr.p_filesz);

        if (to_zero) {
            void *ptr = (void *)(uintptr_t)(load_vaddr + phdr.p_filesz);
            memset(ptr, 0, to_zero);
        }

        if (elf64_apply_relocations(elf, &hdr, (void *)(uintptr_t)load_vaddr, phdr.p_vaddr, phdr.p_memsz, slide))
            return -1;
    }

    if (simulation) {
        simulation = false;
        goto final;
    }

    *entry_point = hdr.entry + slide;
    *_slide = slide;

    return 0;
}

int elf32_load(uint8_t *elf, uint32_t *entry_point, uint32_t alloc_type) {
    struct elf32_hdr hdr;
    memcpy(&hdr, elf + (0), sizeof(struct elf32_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        print("elf: Not a valid ELF file.\n");
        return -1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        print("elf: Not a Little-endian ELF file.\n");
        return -1;
    }

    if (hdr.machine != ARCH_X86_32) {
        print("elf: Not an x86_32 ELF file.\n");
        return -1;
    }

    uint32_t entry = hdr.entry;
    bool entry_adjusted = false;

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf32_phdr phdr;
        memcpy(&phdr, elf + (hdr.phoff + i * sizeof(struct elf32_phdr)),
                   sizeof(struct elf32_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        memmap_alloc_range((size_t)phdr.p_paddr, (size_t)phdr.p_memsz, alloc_type, true, true, false, false);

        memcpy((void *)(uintptr_t)phdr.p_paddr, elf + (phdr.p_offset), phdr.p_filesz);

        size_t to_zero = (size_t)(phdr.p_memsz - phdr.p_filesz);

        if (to_zero) {
            void *ptr = (void *)(uintptr_t)(phdr.p_paddr + phdr.p_filesz);
            memset(ptr, 0, to_zero);
        }

        if (!entry_adjusted && entry >= phdr.p_vaddr && entry <= (phdr.p_vaddr + phdr.p_memsz)) {
            entry -= phdr.p_vaddr;
            entry += phdr.p_paddr;
            entry_adjusted = true;
        }
    }

    *entry_point = entry;

    return 0;
}
