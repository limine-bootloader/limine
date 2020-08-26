#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/stivale.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/memmap.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/print.h>
#include <lib/rand.h>
#include <lib/real.h>
#include <drivers/vbe.h>
#include <drivers/vga_textmode.h>
#include <fs/file.h>
#include <lib/asm.h>

struct stivale_header {
    uint64_t stack;
    uint16_t flags;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
    uint64_t entry_point;
} __attribute__((packed));

struct stivale_module {
    uint64_t begin;
    uint64_t end;
    char     string[128];
    uint64_t next;
} __attribute__((packed));

struct stivale_struct {
    uint64_t cmdline;
    uint64_t memory_map_addr;
    uint64_t memory_map_entries;
    uint64_t framebuffer_addr;
    uint16_t framebuffer_pitch;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
    uint64_t rsdp;
    uint64_t module_count;
    uint64_t modules;
    uint64_t epoch;
    uint64_t flags;       // bit 0: 1 if booted with BIOS, 0 if booted with UEFI
} __attribute__((packed));

#define KASLR_SLIDE_BITMASK 0x03FFFF000u

struct stivale_struct stivale_struct = {0};

void stivale_load(char *cmdline, int boot_drive) {
    stivale_struct.flags |= (1 << 0);  // set bit 0 since we are BIOS and not UEFI

    int kernel_drive; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "KERNEL_DRIVE")) {
            kernel_drive = boot_drive;
        } else {
            kernel_drive = (int)strtoui(buf);
        }
    }

    int kernel_part; {
        char buf[32];
        if (!config_get_value(buf, 0, 32, "KERNEL_PARTITION")) {
            panic("KERNEL_PARTITION not specified");
        } else {
            kernel_part = (int)strtoui(buf);
        }
    }

    char *kernel_path = balloc(128);
    if (!config_get_value(kernel_path, 0, 128, "KERNEL_PATH")) {
        panic("KERNEL_PATH not specified");
    }

    struct file_handle *fd = balloc(sizeof(struct file_handle));
    if (fopen(fd, kernel_drive, kernel_part, kernel_path)) {
        panic("Could not open kernel file");
    }

    struct stivale_header stivale_hdr;

    int bits = elf_bits(fd);

    int ret;

    uint64_t slide = 0;

    bool level5pg = false;
    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
            if (!(edx & (1 << 29))) {
                panic("stivale: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx);
            if (ecx & (1 << 16)) {
                print("stivale: CPU has 5-level paging support\n");
                level5pg = true;
            }

            ret = elf64_load_section(fd, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header), slide);

            if (!ret && ((stivale_hdr.flags >> 2) & 1)) {
                // KASLR is enabled, set the slide
                slide = rand64() & KASLR_SLIDE_BITMASK;

                // Re-read the .stivalehdr with slid relocations
                ret = elf64_load_section(fd, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header), slide);
            }

            break;
        }
        case 32:
            ret = elf32_load_section(fd, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header));
            break;
        default:
            panic("stivale: Not 32 nor 64 bit x86 ELF file.");
    }

    print("stivale: %u-bit ELF file detected\n", bits);

    switch (ret) {
        case 1:
            panic("stivale: File is not a valid ELF.");
        case 2:
            panic("stivale: Section .stivalehdr not found.");
        case 3:
            panic("stivale: Section .stivalehdr exceeds the size of the struct.");
        case 4:
            panic("stivale: Section .stivalehdr is smaller than size of the struct.");
    }

    print("stivale: Requested stack at %X\n", stivale_hdr.stack);

    uint64_t entry_point   = 0;
    uint64_t top_used_addr = 0;

    switch (bits) {
        case 64:
            elf64_load(fd, &entry_point, &top_used_addr, slide, 10);
            break;
        case 32:
            elf32_load(fd, (uint32_t *)&entry_point, (uint32_t *)&top_used_addr, 10);
            break;
    }

    if (stivale_hdr.entry_point != 0)
        entry_point = stivale_hdr.entry_point;

    print("stivale: Kernel slide: %X\n", slide);

    print("stivale: Top used address in ELF: %X\n", top_used_addr);

    stivale_struct.module_count = 0;
    uint64_t *prev_mod_ptr = &stivale_struct.modules;
    for (int i = 0; ; i++) {
        char module_file[64];
        if (!config_get_value(module_file, i, 64, "MODULE_PATH"))
            break;

        stivale_struct.module_count++;

        struct stivale_module *m = balloc(sizeof(struct stivale_module));

        if (!config_get_value(m->string, i, 128, "MODULE_STRING")) {
            m->string[0] = '\0';
        }

        int part; {
            char buf[32];
            if (!config_get_value(buf, i, 32, "MODULE_PARTITION")) {
                part = kernel_part;
            } else {
                part = (int)strtoui(buf);
            }
        }

        struct file_handle f;
        if (fopen(&f, fd->disk, part, module_file)) {
            panic("Requested module with path \"%s\" not found!\n", module_file);
        }

        void *module_addr = (void *)(((uint32_t)top_used_addr & 0xfff) ?
            ((uint32_t)top_used_addr & ~((uint32_t)0xfff)) + 0x1000 :
            (uint32_t)top_used_addr);

        memmap_alloc_range((size_t)module_addr, f.size, 10);
        fread(&f, module_addr, 0, f.size);

        m->begin = (uint64_t)(size_t)module_addr;
        m->end   = m->begin + f.size;
        m->next  = 0;

        top_used_addr = (uint64_t)(size_t)m->end;

        *prev_mod_ptr = (uint64_t)(size_t)m;
        prev_mod_ptr  = &m->next;

        print("stivale: Requested module %u:\n", i);
        print("         Path:   %s\n", module_file);
        print("         String: %s\n", m->string);
        print("         Begin:  %X\n", m->begin);
        print("         End:    %X\n", m->end);
    }

    stivale_struct.rsdp = (uint64_t)(size_t)get_rsdp();

    stivale_struct.cmdline = (uint64_t)(size_t)cmdline;

    stivale_struct.epoch = time();
    print("stivale: Current epoch: %U\n", stivale_struct.epoch);

    stivale_struct.framebuffer_width  = stivale_hdr.framebuffer_width;
    stivale_struct.framebuffer_height = stivale_hdr.framebuffer_height;
    stivale_struct.framebuffer_bpp    = stivale_hdr.framebuffer_bpp;

    if (stivale_hdr.flags & (1 << 0)) {
        init_vbe(&stivale_struct.framebuffer_addr,
                 &stivale_struct.framebuffer_pitch,
                 &stivale_struct.framebuffer_width,
                 &stivale_struct.framebuffer_height,
                 &stivale_struct.framebuffer_bpp);
    } else {
        deinit_vga_textmode();
    }

    size_t memmap_entries;
    struct e820_entry_t *memmap = get_memmap(&memmap_entries);
    stivale_struct.memory_map_entries = (uint64_t)memmap_entries;
    stivale_struct.memory_map_addr    = (uint64_t)(size_t)memmap;

    if (bits == 64) {
        // If we're going 64, we might as well call this BIOS interrupt
        // to tell the BIOS that we are entering Long Mode, since it is in
        // the specification.
        struct rm_regs r = {0};
        r.eax = 0xec00;
        r.ebx = 0x02;   // Long mode only
        rm_int(0x15, &r, &r);
    }

    rm_flush_irqs();

    if (bits == 64) {
        void *pagemap_ptr;
        if (level5pg && (stivale_hdr.flags & (1 << 1))) {
            // Enable CR4.LA57
            ASM(
                "mov eax, cr4\n\t"
                "bts eax, 12\n\t"
                "mov cr4, eax\n\t", :: "eax", "memory"
            );

            struct pagemap {
                uint64_t pml5[512];
                uint64_t pml4_lo[512];
                uint64_t pml4_hi[512];
                uint64_t pml3_lo[512];
                uint64_t pml3_hi[512];
                uint64_t pml2_0gb[512];
                uint64_t pml2_1gb[512];
                uint64_t pml2_2gb[512];
                uint64_t pml2_3gb[512];
            };
            struct pagemap *pagemap = balloc_aligned(sizeof(struct pagemap), 0x1000);
            pagemap_ptr = (void *)pagemap;

            // zero out the pagemap
            for (uint64_t *p = (uint64_t *)pagemap; p < &pagemap->pml3_hi[512]; p++)
                *p = 0;

            pagemap->pml5[511]    = (uint64_t)(size_t)pagemap->pml4_hi  | 0x03;
            pagemap->pml5[0]      = (uint64_t)(size_t)pagemap->pml4_lo  | 0x03;
            pagemap->pml4_hi[511] = (uint64_t)(size_t)pagemap->pml3_hi  | 0x03;
            pagemap->pml4_hi[256] = (uint64_t)(size_t)pagemap->pml3_lo  | 0x03;
            pagemap->pml4_lo[0]   = (uint64_t)(size_t)pagemap->pml3_lo  | 0x03;
            pagemap->pml3_hi[510] = (uint64_t)(size_t)pagemap->pml2_0gb | 0x03;
            pagemap->pml3_hi[511] = (uint64_t)(size_t)pagemap->pml2_1gb | 0x03;
            pagemap->pml3_lo[0]   = (uint64_t)(size_t)pagemap->pml2_0gb | 0x03;
            pagemap->pml3_lo[1]   = (uint64_t)(size_t)pagemap->pml2_1gb | 0x03;
            pagemap->pml3_lo[2]   = (uint64_t)(size_t)pagemap->pml2_2gb | 0x03;
            pagemap->pml3_lo[3]   = (uint64_t)(size_t)pagemap->pml2_3gb | 0x03;

            // populate the page directories
            for (size_t i = 0; i < 512 * 4; i++)
                (&pagemap->pml2_0gb[0])[i] = (i * 0x200000) | 0x03 | (1 << 7);
        } else {
            struct pagemap {
                uint64_t pml4[512];
                uint64_t pml3_lo[512];
                uint64_t pml3_hi[512];
                uint64_t pml2_0gb[512];
                uint64_t pml2_1gb[512];
                uint64_t pml2_2gb[512];
                uint64_t pml2_3gb[512];
            };
            struct pagemap *pagemap = balloc_aligned(sizeof(struct pagemap), 0x1000);
            pagemap_ptr = (void *)pagemap;

            // zero out the pagemap
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
                (&pagemap->pml2_0gb[0])[i] = (i * 0x200000) | 0x03 | (1 << 7);
        }

        ASM(
            "cli\n\t"
            "cld\n\t"
            "mov cr3, eax\n\t"
            "mov eax, cr4\n\t"
            "or eax, 1 << 5\n\t"
            "mov cr4, eax\n\t"
            "mov ecx, 0xc0000080\n\t"
            "rdmsr\n\t"
            "or eax, 1 << 8\n\t"
            "wrmsr\n\t"
            "mov eax, cr0\n\t"
            "or eax, 1 << 31\n\t"
            "mov cr0, eax\n\t"
            FARJMP32("0x28", "1f")
            "1: .code64\n\t"
            "mov ax, 0x30\n\t"
            "mov ds, ax\n\t"
            "mov es, ax\n\t"
            "mov fs, ax\n\t"
            "mov gs, ax\n\t"
            "mov ss, ax\n\t"

            "push 0x30\n\t"
            "push [rsi]\n\t"
            "pushfq\n\t"
            "push 0x28\n\t"
            "push [rbx]\n\t"

            "xor rax, rax\n\t"
            "xor rbx, rbx\n\t"
            "xor rcx, rcx\n\t"
            "xor rdx, rdx\n\t"
            "xor rsi, rsi\n\t"
            "xor rbp, rbp\n\t"
            "xor r8,  r8\n\t"
            "xor r9,  r9\n\t"
            "xor r10, r10\n\t"
            "xor r11, r11\n\t"
            "xor r12, r12\n\t"
            "xor r13, r13\n\t"
            "xor r14, r14\n\t"
            "xor r15, r15\n\t"

            "iretq\n\t"
            ".code32\n\t",
            : "a" (pagemap_ptr), "b" (&entry_point),
              "D" (&stivale_struct), "S" (&stivale_hdr.stack)
            : "memory"
        );
    } else if (bits == 32) {
        ASM(
            "cli\n\t"
            "cld\n\t"

            "sub esp, 4\n\t"
            "mov [esp], edi\n\t"

            "push 0x20\n\t"
            "push [esi]\n\t"
            "pushfd\n\t"
            "push 0x18\n\t"
            "push [ebx]\n\t"

            "xor eax, eax\n\t"
            "xor ebx, ebx\n\t"
            "xor ecx, ecx\n\t"
            "xor edx, edx\n\t"
            "xor esi, esi\n\t"
            "xor edi, edi\n\t"
            "xor ebp, ebp\n\t"

            "iret\n\t",
            : "b" (&entry_point), "D" (&stivale_struct), "S" (&stivale_hdr.stack)
            : "memory"
        );
    }
}
