#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <limine.h>
#include <protos/stivale.h>
#include <protos/stivale2.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/print.h>
#include <lib/rand.h>
#include <lib/real.h>
#include <lib/libc.h>
#include <sys/smp.h>
#include <sys/cpu.h>
#include <drivers/vbe.h>
#include <lib/term.h>
#include <sys/pic.h>
#include <sys/lapic.h>
#include <fs/file.h>
#include <mm/pmm.h>
#include <stivale/stivale2.h>

#define KASLR_SLIDE_BITMASK 0x03FFFF000u

struct stivale2_struct stivale2_struct = {0};

inline static size_t get_phys_addr(uint64_t addr) {
    if (addr & ((uint64_t)1 << 63))
        return addr - FIXED_HIGHER_HALF_OFFSET_64;
    return addr;
}

static void *get_tag(struct stivale2_header *s, uint64_t id) {
    struct stivale2_tag *tag = (void*)get_phys_addr(s->tags);
    for (;;) {
        if (tag == NULL)
            return NULL;
        if (tag->identifier == id)
            return tag;
        tag = (void*)get_phys_addr(tag->next);
    }
}

static void append_tag(struct stivale2_struct *s, struct stivale2_tag *tag) {
    tag->next = s->tags;
    s->tags   = (uint64_t)(size_t)tag;
}

void stivale2_load(char *cmdline, int boot_drive) {
    struct kernel_loc kernel = get_kernel_loc(boot_drive);

    struct stivale2_header stivale2_hdr;

    int bits = elf_bits(kernel.fd);

    int ret;

    uint64_t slide = 0;

    bool level5pg = false;
    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx);
            if (!(edx & (1 << 29))) {
                panic("stivale2: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx);
            if (ecx & (1 << 16)) {
                print("stivale2: CPU has 5-level paging support\n");
                level5pg = true;
            }

            ret = elf64_load_section(kernel.fd, &stivale2_hdr, ".stivale2hdr", sizeof(struct stivale2_header), slide);

            if (!ret && (stivale2_hdr.flags & 1)) {
                // KASLR is enabled, set the slide
                slide = rand64() & KASLR_SLIDE_BITMASK;

                // Re-read the .stivale2hdr with slid relocations
                ret = elf64_load_section(kernel.fd, &stivale2_hdr, ".stivale2hdr", sizeof(struct stivale2_header), slide);
            }

            break;
        }
        case 32:
            ret = elf32_load_section(kernel.fd, &stivale2_hdr, ".stivale2hdr", sizeof(struct stivale2_header));
            break;
        default:
            panic("stivale2: Not 32 nor 64 bit x86 ELF file.");
    }

    print("stivale2: %u-bit ELF file detected\n", bits);

    switch (ret) {
        case 1:
            panic("stivale2: File is not a valid ELF.");
        case 2:
            panic("stivale2: Section .stivale2hdr not found.");
        case 3:
            panic("stivale2: Section .stivale2hdr exceeds the size of the struct.");
        case 4:
            panic("stivale2: Section .stivale2hdr is smaller than size of the struct.");
    }

    print("stivale2: Requested stack at %X\n", stivale2_hdr.stack);

    uint64_t entry_point   = 0;
    uint64_t top_used_addr = 0;

    switch (bits) {
        case 64:
            elf64_load(kernel.fd, &entry_point, &top_used_addr, slide, 0x1001);
            break;
        case 32:
            elf32_load(kernel.fd, (uint32_t *)&entry_point, (uint32_t *)&top_used_addr, 0x1001);
            break;
    }

    if (stivale2_hdr.entry_point != 0)
        entry_point = stivale2_hdr.entry_point;

    print("stivale2: Kernel slide: %X\n", slide);

    print("stivale2: Top used address in ELF: %X\n", top_used_addr);

    strcpy(stivale2_struct.bootloader_brand, "Limine");
    strcpy(stivale2_struct.bootloader_version, LIMINE_VERSION);

    //////////////////////////////////////////////
    // Create firmware struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_firmware *tag = conv_mem_alloc(sizeof(struct stivale2_struct_tag_firmware));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_FIRMWARE_ID;

    tag->flags = 1 << 0;   // bit 0 = BIOS boot

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create modules struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_modules *tag = conv_mem_alloc(sizeof(struct stivale2_struct_tag_modules));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_MODULES_ID;

    tag->module_count = 0;

    for (int i = 0; ; i++) {
        char module_file[64];
        if (!config_get_value(module_file, i, 64, "MODULE_PATH"))
            break;

        tag->module_count++;

        struct stivale2_module *m = conv_mem_alloc_aligned(sizeof(struct stivale2_module), 1);

        if (!config_get_value(m->string, i, 128, "MODULE_STRING")) {
            m->string[0] = '\0';
        }

        int part; {
            char buf[32];
            if (!config_get_value(buf, i, 32, "MODULE_PARTITION")) {
                part = kernel.kernel_part;
            } else {
                part = (int)strtoui(buf);
            }
        }

        struct file_handle f;
        if (fopen(&f, kernel.fd->disk, part, module_file)) {
            panic("Requested module with path \"%s\" not found!\n", module_file);
        }

        void *module_addr = (void *)(((uint32_t)top_used_addr & 0xfff) ?
            ((uint32_t)top_used_addr & ~((uint32_t)0xfff)) + 0x1000 :
            (uint32_t)top_used_addr);

        print("stivale2: Loading module `%s`...\n", module_file);

        memmap_alloc_range((size_t)module_addr, f.size, 0x1001);
        fread(&f, module_addr, 0, f.size);

        m->begin = (uint64_t)(size_t)module_addr;
        m->end   = m->begin + f.size;

        top_used_addr = (uint64_t)(size_t)m->end;

        print("stivale2: Requested module %u:\n", i);
        print("          Path:   %s\n", module_file);
        print("          String: %s\n", m->string);
        print("          Begin:  %X\n", m->begin);
        print("          End:    %X\n", m->end);
    }

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create RSDP struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_rsdp *tag = conv_mem_alloc(sizeof(struct stivale2_struct_tag_rsdp));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_RSDP_ID;

    tag->rsdp = (uint64_t)(size_t)acpi_get_rsdp();

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create cmdline struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_cmdline *tag = conv_mem_alloc(sizeof(struct stivale2_struct_tag_cmdline));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_CMDLINE_ID;

    tag->cmdline = (uint64_t)(size_t)cmdline;

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create epoch struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_epoch *tag = conv_mem_alloc(sizeof(struct stivale2_struct_tag_epoch));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_EPOCH_ID;

    tag->epoch = time();
    print("stivale2: Current epoch: %U\n", tag->epoch);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create framebuffer struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_header_tag_framebuffer *hdrtag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_FRAMEBUFFER_ID);

    term_deinit();

    if (hdrtag != NULL) {
        struct stivale2_struct_tag_framebuffer *tag = conv_mem_alloc(sizeof(struct stivale2_struct_tag_framebuffer));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID;

        tag->framebuffer_width  = hdrtag->framebuffer_width;
        tag->framebuffer_height = hdrtag->framebuffer_height;
        tag->framebuffer_bpp    = hdrtag->framebuffer_bpp;

        uint32_t *fb32;
        init_vbe(&fb32,
                 &tag->framebuffer_pitch,
                 &tag->framebuffer_width,
                 &tag->framebuffer_height,
                 &tag->framebuffer_bpp);
        tag->framebuffer_addr = (uint64_t)(size_t)fb32;

        append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

    size_t memmap_entries;
    struct e820_entry_t *memmap = get_memmap(&memmap_entries);

    // Check if 5-level paging tag is requesting support
    bool level5pg_requested = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_5LV_PAGING_ID) ? true : false;

    pagemap_t pagemap = {0};
    if (bits == 64)
        pagemap = stivale_build_pagemap(level5pg && level5pg_requested,
                                        memmap, memmap_entries);

    //////////////////////////////////////////////
    // Create memmap struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_memmap *tag = conv_mem_alloc(sizeof(struct stivale2_struct_tag_memmap));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_MEMMAP_ID;

    memmap = get_memmap(&memmap_entries);

    tag->entries = (uint64_t)memmap_entries;

    void *tag_memmap = conv_mem_alloc_aligned(sizeof(struct e820_entry_t) * memmap_entries, 1);
    memcpy(tag_memmap, memmap, sizeof(struct e820_entry_t) * memmap_entries);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create SMP struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_header_tag_smp *smp_hdr_tag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_SMP_ID);
    if (smp_hdr_tag != NULL) {
        struct smp_information *smp_info;
        size_t cpu_count;
        smp_info = init_smp(&cpu_count, bits == 64, level5pg && level5pg_requested,
                            pagemap, smp_hdr_tag->flags & 1);

        struct stivale2_struct_tag_smp *tag =
            conv_mem_alloc(sizeof(struct stivale2_struct_tag_smp)
                         + sizeof(struct smp_information) * cpu_count);
        tag->tag.identifier = STIVALE2_STRUCT_TAG_SMP_ID;
        tag->cpu_count      = cpu_count;
        tag->flags         |= (smp_hdr_tag->flags & 1) && x2apic_check();

        memcpy((void*)tag + sizeof(struct stivale2_struct_tag_smp),
               smp_info, sizeof(struct smp_information) * cpu_count);

        append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

    print("Generated tags:\n");
    struct stivale2_tag *taglist = (void*)(size_t)stivale2_struct.tags;
    for (size_t i = 0; ; i++) {
        print("Tag #%u  ID: %X\n", i, taglist->identifier);
        if (taglist->next)
            taglist = (void*)(size_t)taglist->next;
        else
            break;
    }

    stivale_spinup(bits, level5pg && level5pg_requested, pagemap,
                   entry_point, &stivale2_struct, stivale2_hdr.stack);
}
