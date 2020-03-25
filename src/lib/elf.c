#include <stdint.h>
#include <stddef.h>
#include <lib/blib.h>
#include <lib/libc.h>
#include <lib/elf.h>

#define PT_LOAD     0x00000001
#define PT_INTERP   0x00000003
#define PT_PHDR     0x00000006

#define ABI_SYSV 0x00
#define ARCH_X86_64 0x3e
#define BITS_LE 0x01

/* Indices into identification array */
#define	EI_CLASS	4
#define	EI_DATA		5
#define	EI_VERSION	6
#define	EI_OSABI	7

struct elf_hdr {
    uint8_t ident[16];
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

struct elf_phdr {
    uint32_t p_type;
    uint32_t p_offset;
    uint32_t p_vaddr;
    uint32_t p_paddr;
    uint32_t p_filesz;
    uint32_t p_memsz;
    uint32_t p_flags;
};

int echfs_read(struct echfs_file_handle *file, void *buf, uint64_t loc, uint64_t count);

int elf_load(struct echfs_file_handle *fd) {
    struct elf_hdr hdr;
    echfs_read(fd, &hdr, 0, sizeof(struct elf_hdr));

    if (strncmp((char *)hdr.ident, "\177ELF", 4)) {
        print("Not a valid ELF file.\n");
        return 1;
    }

    if (hdr.ident[EI_DATA] != BITS_LE) {
        print("Not a Little-endian ELF file.\n");
        return -1;
    }

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf_phdr phdr;
        echfs_read(fd, &phdr, hdr.phoff + i * sizeof(struct elf_phdr),
                   sizeof(struct elf_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;


        echfs_read(fd, (void *)phdr.p_vaddr, phdr.p_offset, phdr.p_filesz);

        size_t to_zero = (size_t)(phdr.p_memsz - phdr.p_filesz);

        if (to_zero) {
            void *ptr = (void *)(phdr.p_vaddr + phdr.p_filesz);
            memset(ptr, 0, to_zero);
        }
    }

    asm volatile (
        "jmp %0\n\t"
        :
        : "r" (hdr.entry)
        : "memory"
    );
}
