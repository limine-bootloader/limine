#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/stivale.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/e820.h>
#include <lib/config.h>
#include <lib/time.h>
#include <drivers/vbe.h>
#include <drivers/vga_textmode.h>
#include <fs/file.h>

struct stivale_header {
    uint64_t stack;
    // Flags
    // bit 0   0 = text mode,   1 = graphics mode
    uint16_t flags;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
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
} __attribute__((packed));

struct stivale_struct stivale_struct = {0};

void stivale_load(char *cmdline, int boot_drive) {
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
            ret = elf64_load_section(fd, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header));
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
            panic("stivale: File is not a valid ELF.\n");
        case 2:
            panic("stivale: Section .stivalehdr not found.\n");
        case 3:
            panic("stivale: Section .stivalehdr exceeds the size of the struct.\n");
    }

    print("stivale: Requested stack at %X\n", stivale_hdr.stack);

    uint64_t entry_point   = 0;
    uint64_t top_used_addr = 0;
    switch (bits) {
        case 64:
            elf64_load(fd, &entry_point, &top_used_addr);
            break;
        case 32:
            elf32_load(fd, (uint32_t *)&entry_point, (uint32_t *)&top_used_addr);
            break;
    }

    print("stivale: Top used address in ELF: %X\n", top_used_addr);

    stivale_struct.memory_map_entries = (uint64_t)e820_entries;
    stivale_struct.memory_map_addr    = (uint64_t)(size_t)e820_map;

    stivale_struct.module_count = 0;
    uint64_t *prev_mod_ptr = &stivale_struct.modules;
    for (int i = 0; ; i++) {
        char module_file[64];
        if (!config_get_value(module_file, i, 64, "MODULE_PATH"))
            break;

        stivale_struct.module_count++;

        struct stivale_module *m = balloc(sizeof(struct stivale_module));

        config_get_value(m->string, i, 128, "MODULE_STRING");

        int part; {
            char buf[32];
            config_get_value(buf, i, 32, "MODULE_PARTITION");
            part = (int)strtoui(buf);
        }

        struct file_handle f;
        fopen(&f, fd->disk, part, module_file);

        void *module_addr = (void *)(((uint32_t)top_used_addr & 0xfff) ?
            ((uint32_t)top_used_addr & ~((uint32_t)0xfff)) + 0x1000 :
            (uint32_t)top_used_addr);

        is_valid_memory_range((size_t)module_addr, f.size);
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

    if (bits == 64) {
        void *pagemap_ptr;
        if (level5pg && (stivale_hdr.flags & (1 << 1))) {
            // Enable CR4.LA57
            asm volatile (
                "mov eax, cr4\n\t"
                "bts eax, 12\n\t"
                "mov cr4, eax\n\t"
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

        asm volatile (
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
            "jmp 0x28:1f\n\t"
            "1: .code64\n\t"
            "mov ax, 0x30\n\t"
            "mov ds, ax\n\t"
            "mov es, ax\n\t"
            "mov fs, ax\n\t"
            "mov gs, ax\n\t"
            "mov ss, ax\n\t"
            "mov rsp, [rsi]\n\t"
            "call [rbx]\n\t"
            ".code32\n\t"
            :
            : "a" (pagemap_ptr), "b" (&entry_point),
              "D" (&stivale_struct), "S" (&stivale_hdr.stack)
        );
    } else if (bits == 32) {
        asm volatile (
            "cli\n\t"
            "cld\n\t"
            "mov esp, [esi]\n\t"
            "push edi\n\t"
            "call [ebx]\n\t"
            :
            : "b" (&entry_point), "D" (&stivale_struct), "S" (&stivale_hdr.stack)
        );
    }
}
