#include <stdint.h>
#include <stddef.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <fs/file.h>

#define PT_LOAD     0x00000001
#define PT_INTERP   0x00000003
#define PT_PHDR     0x00000006

#define ABI_SYSV 0x00
#define ARCH_X86_64 0x3e
#define BITS_LE 0x01

/* Indices into identification array */
#define EI_CLASS    4
#define EI_DATA     5
#define EI_VERSION  6
#define EI_OSABI    7

struct elf_hdr {
    uint8_t ident[16];
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

struct elf_phdr {
    uint32_t p_type;
    uint32_t p_flags;
    uint64_t p_offset;
    uint64_t p_vaddr;
    uint64_t p_paddr;
    uint64_t p_filesz;
    uint64_t p_memsz;
    uint64_t p_align;
};

struct elf_shdr {
    uint32_t   sh_name;
    uint32_t   sh_type;
    uint64_t   sh_flags;
    uint64_t   sh_addr;
    uint64_t   sh_offset;
    uint64_t   sh_size;
    uint32_t   sh_link;
    uint32_t   sh_info;
    uint64_t   sh_addralign;
    uint64_t   sh_entsize;
};

int elf_load_section(struct file_handle *fd, void *buffer, const char *name, size_t limit) {
    struct elf_hdr hdr;
    fread(fd, &hdr, 0, sizeof(struct elf_hdr));

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

    struct elf_shdr shstrtab;
    fread(fd, &shstrtab, hdr.shoff + hdr.shstrndx * sizeof(struct elf_shdr),
            sizeof(struct elf_shdr));

    char names[shstrtab.sh_size];
    fread(fd, names, shstrtab.sh_offset, shstrtab.sh_size);

    for (uint16_t i = 0; i < hdr.sh_num; i++) {
        struct elf_shdr section;
        fread(fd, &section, hdr.shoff + i * sizeof(struct elf_shdr),
                   sizeof(struct elf_shdr));

        if (!strcmp(&names[section.sh_name], name)) {
            if (section.sh_size > limit)
                return 3;
            fread(fd, buffer, section.sh_offset, section.sh_size);
            return 0;
        }
    }

    return 2;
}

#define FIXED_HIGHER_HALF_OFFSET ((uint64_t)0xffffffff80000000)

int elf_load(struct file_handle *fd, uint64_t *entry_point, uint64_t *top) {
    struct elf_hdr hdr;
    fread(fd, &hdr, 0, sizeof(struct elf_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        print("Not a valid ELF file.\n");
        return -1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        print("Not a Little-endian ELF file.\n");
        return -1;
    }

    if (hdr.machine != ARCH_X86_64) {
        print("Not an x86_64 ELF file.\n");
        return -1;
    }

    *top = 0;

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf_phdr phdr;
        fread(fd, &phdr, hdr.phoff + i * sizeof(struct elf_phdr),
                   sizeof(struct elf_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        if (phdr.p_vaddr & (1ull << 63))
            phdr.p_vaddr -= FIXED_HIGHER_HALF_OFFSET;

        uint64_t this_top = phdr.p_vaddr + phdr.p_memsz;
        if (this_top > *top)
            *top = this_top;

        fread(fd, (void *)(uint32_t)phdr.p_vaddr,
                   phdr.p_offset, phdr.p_filesz);

        size_t to_zero = (size_t)(phdr.p_memsz - phdr.p_filesz);

        if (to_zero) {
            void *ptr = (void *)(uint32_t)(phdr.p_vaddr + phdr.p_filesz);
            memset(ptr, 0, to_zero);
        }
    }

    *entry_point = hdr.entry;

    return 0;
}
