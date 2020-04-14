#include <stdint.h>
#include <stddef.h>
#include <protos/stivale.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/e820.h>
#include <lib/config.h>
#include <drivers/vbe.h>
<<<<<<< HEAD
=======
#include <fs/file.h>
>>>>>>> upstream/master

struct stivale_header {
    uint64_t stack;
    uint16_t video_mode;  // 0 = default at boot (CGA text mode). 1 = graphical VESA
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
} __attribute__((packed));

struct stivale_struct stivale_struct = {0};

void stivale_load(struct file_handle *fd, char *cmdline) {
    uint64_t entry_point;

    struct stivale_header stivale_hdr;
    int ret = elf_load_section(fd, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header));
    switch (ret) {
        case 1:
            print("stivale: File is not a valid ELF.\n");
            for (;;);
        case 2:
            print("stivale: Section .stivalehdr not found.\n");
            for (;;);
        case 3:
            print("stivale: Section .stivalehdr exceeds the size of the struct.\n");
            for (;;);
        default:
            break;
    }

    print("stivale: Requested stack at %X\n", stivale_hdr.stack);
    print("stivale: Video mode: %u\n", stivale_hdr.video_mode);

    uint64_t top_used_addr;
    elf_load(fd, &entry_point, &top_used_addr);

    print("stivale: Top used address in ELF: %X\n", top_used_addr);

    stivale_struct.memory_map_entries = (uint64_t)init_e820();
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

<<<<<<< HEAD
        FILE *f = bfopen(module_file, fd->drive, fd->part);
=======
        struct file_handle f;
        fopen(&f, fd->disk, part, module_file);
>>>>>>> upstream/master

        void *module_addr = (void *)(((uint32_t)top_used_addr & 0xfff) ?
            ((uint32_t)top_used_addr & ~((uint32_t)0xfff)) + 0x1000 :
            (uint32_t)top_used_addr);

<<<<<<< HEAD
        bfgets(module_addr, 0, f->size, f);

        m->begin = (uint64_t)(size_t)module_addr;
        m->end   = m->begin + f->size;
=======
        fread(&f, module_addr, 0, f.size);

        m->begin = (uint64_t)(size_t)module_addr;
        m->end   = m->begin + f.size;
>>>>>>> upstream/master
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
    print("stivale: RSDP at %X\n", stivale_struct.rsdp);

    stivale_struct.cmdline = (uint64_t)(size_t)cmdline;

    stivale_struct.framebuffer_width  = stivale_hdr.framebuffer_width;
    stivale_struct.framebuffer_height = stivale_hdr.framebuffer_height;
    stivale_struct.framebuffer_bpp    = stivale_hdr.framebuffer_bpp;

    if (stivale_hdr.video_mode == 1) {
        init_vbe(&stivale_struct.framebuffer_addr,
                 &stivale_struct.framebuffer_pitch,
                 &stivale_struct.framebuffer_width,
                 &stivale_struct.framebuffer_height,
                 &stivale_struct.framebuffer_bpp);
    }

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
        (&pagemap->pml2_0gb[0])[i] = (i * 0x200000) | 0x03 | (1 << 7);

    asm volatile (
        "cli\n\t"
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
        "jmp [rbx]\n\t"
        ".code32\n\t"
        :
        : "a" (pagemap), "b" (&entry_point),
          "D" (&stivale_struct), "S" (&stivale_hdr.stack)
    );
}
