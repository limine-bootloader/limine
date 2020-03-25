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

#define FIXED_HIGHER_HALF_OFFSET ((uint64_t)0xffffffff80000000)

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

    if (hdr.machine != ARCH_X86_64) {
        print("Not an x86_64 ELF file.\n");
        return -1;
    }

    for (uint16_t i = 0; i < hdr.ph_num; i++) {
        struct elf_phdr phdr;
        echfs_read(fd, &phdr, hdr.phoff + i * sizeof(struct elf_phdr),
                   sizeof(struct elf_phdr));

        if (phdr.p_type != PT_LOAD)
            continue;

        if (phdr.p_vaddr & (1ull << 63))
            phdr.p_vaddr -= FIXED_HIGHER_HALF_OFFSET;

        echfs_read(fd, (void *)(uint32_t)phdr.p_vaddr,
                   phdr.p_offset, phdr.p_filesz);

        size_t to_zero = (size_t)(phdr.p_memsz - phdr.p_filesz);

        if (to_zero) {
            void *ptr = (void *)(uint32_t)(phdr.p_vaddr + phdr.p_filesz);
            memset(ptr, 0, to_zero);
        }
    }

    volatile struct {
        uint64_t pml4[512];
        uint64_t pml3_lo[512];
        uint64_t pml3_hi[512];
        uint64_t pml2_0gb[512];
        uint64_t pml2_1gb[512];
        uint64_t pml2_2gb[512];
        uint64_t pml2_3gb[512];
    } *pagemap = (void *)0x10000;

    // first, zero out the pagemap
    for (uint64_t *p = (uint64_t *)pagemap; p < &pagemap->pml3_hi[512]; p++)
        *p = 0;

    pagemap->pml4[511]    = (uint64_t)(size_t)pagemap->pml3_hi  | 0x03;
    pagemap->pml4[256]    = (uint64_t)(size_t)pagemap->pml3_lo  | 0x03;
    pagemap->pml4[0]      = (uint64_t)(size_t)pagemap->pml3_lo  | 0x03;
    pagemap->pml3_hi[510] = (uint64_t)(size_t)pagemap->pml2_0gb | 0x03;
    pagemap->pml3_hi[511] = (uint64_t)(size_t)pagemap->pml2_1gb | 0x03;
    pagemap->pml3_lo[0]   = (uint64_t)(size_t)pagemap->pml2_0gb | 0x03;
    pagemap->pml3_lo[1]   = (uint64_t)(size_t)pagemap->pml2_1gb | 0x03;
    pagemap->pml3_lo[2]   = (uint64_t)(size_t)pagemap->pml2_2gb | 0x03;
    pagemap->pml3_lo[3]   = (uint64_t)(size_t)pagemap->pml2_3gb | 0x03;

    // populate the page directories
    for (size_t i = 0; i < 512 * 4; i++)
        (&pagemap->pml2_0gb[0])[i] = (i * 0x1000) | 0x03 | (1 << 7);

    asm volatile (
        "cli\n\t"
        "mov cr3, eax\n\t"
        "mov eax, cr4\n\t"
        "or eax, 1 << 5 | 1 << 7\n\t"
        "mov cr4, eax\n\t"
        "mov ecx, 0xc0000080\n\t"
        "rdmsr\n\t"
        "or eax, 1 << 8\n\t"
        "wrmsr\n\t"
        "mov eax, cr0\n\t"
        "or eax, 1 << 31\n\t"
        "mov cr0, eax\n\t"
        "jmp 0x28:1f\n\t"
        "1: .code64\n\t"
        "mov ax, 0x30\n\t"
        "mov ds, ax\n\t"
        "mov es, ax\n\t"
        "mov fs, ax\n\t"
        "mov gs, ax\n\t"
        "mov ss, ax\n\t"
        "jmp [rbx]\n\t"
        ".code32\n\t"
        :
        : "a" (pagemap), "b" (&hdr.entry)
    );

    return 0;
}
