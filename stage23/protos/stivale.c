#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/stivale.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/print.h>
#include <lib/rand.h>
#include <lib/real.h>
#include <lib/uri.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <sys/pic.h>
#include <sys/cpu.h>
#include <sys/gdt.h>
#include <fs/file.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <mm/mtrr.h>
#include <stivale/stivale.h>

#define KASLR_SLIDE_BITMASK 0x000FFF000u

struct stivale_struct stivale_struct = {0};

void stivale_load(char *config, char *cmdline) {
    stivale_struct.flags |= (1 << 0);  // set bit 0 since we are BIOS and not UEFI
    stivale_struct.flags |= (1 << 1);  // we give colour information

    struct file_handle *kernel = ext_mem_alloc(sizeof(struct file_handle));

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("KERNEL_PATH not specified");

    if (!uri_open(kernel, kernel_path))
        panic("Could not open kernel resource");

    struct stivale_header stivale_hdr;

    int bits = elf_bits(kernel);

    int ret;

    uint64_t slide = 0;

    bool level5pg = false;
    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
                panic("stivale: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
                print("stivale: CPU has 5-level paging support\n");
                level5pg = true;
            }

            char *s_kaslr = config_get_value(config, 0, "KASLR");
            if (s_kaslr != NULL && !strcmp(s_kaslr, "yes")) {
                // KASLR is enabled, set the slide
                slide = rand64() & KASLR_SLIDE_BITMASK;
            }

            ret = elf64_load_section(kernel, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header), slide);

            break;
        }
        case 32:
            ret = elf32_load_section(kernel, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header));
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
            elf64_load(kernel, &entry_point, &top_used_addr, slide, 10);
            break;
        case 32:
            elf32_load(kernel, (uint32_t *)&entry_point, (uint32_t *)&top_used_addr, 10);
            break;
    }

    if (stivale_hdr.entry_point != 0)
        entry_point = stivale_hdr.entry_point;

    print("stivale: Kernel slide: %X\n", slide);

    print("stivale: Top used address in ELF: %X\n", top_used_addr);

    stivale_struct.module_count = 0;
    uint64_t *prev_mod_ptr = &stivale_struct.modules;
    for (int i = 0; ; i++) {
        char *module_path = config_get_value(config, i, "MODULE_PATH");
        if (module_path == NULL)
            break;

        stivale_struct.module_count++;

        struct stivale_module *m = ext_mem_alloc(sizeof(struct stivale_module));

        char *module_string = config_get_value(config, i, "MODULE_STRING");
        if (module_string == NULL) {
            m->string[0] = '\0';
        } else {
            // TODO perhaps change this to be a pointer
            size_t str_len = strlen(module_string);
            if (str_len > 127)
                str_len = 127;
            memcpy(m->string, module_string, str_len);
        }

        print("stivale: Loading module `%s`...\n", module_path);

        struct file_handle f;
        if (!uri_open(&f, module_path))
            panic("Requested module with path \"%s\" not found!", module_path);

        m->begin = (uint64_t)(size_t)freadall(&f, STIVALE_MMAP_KERNEL_AND_MODULES);
        m->end   = m->begin + f.size;
        m->next  = 0;

        *prev_mod_ptr = (uint64_t)(size_t)m;
        prev_mod_ptr  = &m->next;

        print("stivale: Requested module %u:\n", i);
        print("         Path:   %s\n", module_path);
        print("         String: %s\n", m->string);
        print("         Begin:  %X\n", m->begin);
        print("         End:    %X\n", m->end);
    }

    stivale_struct.rsdp = (uint64_t)(size_t)acpi_get_rsdp();

    stivale_struct.cmdline = (uint64_t)(size_t)cmdline;

    stivale_struct.epoch = time();
    print("stivale: Current epoch: %U\n", stivale_struct.epoch);

    term_deinit();

    if (stivale_hdr.flags & (1 << 0)) {
        int req_width  = stivale_hdr.framebuffer_width;
        int req_height = stivale_hdr.framebuffer_height;
        int req_bpp    = stivale_hdr.framebuffer_bpp;

        char *resolution = config_get_value(config, 0, "RESOLUTION");
        if (resolution != NULL)
            parse_resolution(&req_width, &req_height, &req_bpp, resolution);

        struct fb_info fbinfo;
        if (!fb_init(&fbinfo, req_width, req_height, req_bpp))
            panic("stivale: Unable to set video mode");

        stivale_struct.framebuffer_addr    = (uint64_t)fbinfo.framebuffer_addr;
        stivale_struct.framebuffer_width   = fbinfo.framebuffer_width;
        stivale_struct.framebuffer_height  = fbinfo.framebuffer_height;
        stivale_struct.framebuffer_bpp     = fbinfo.framebuffer_bpp;
        stivale_struct.framebuffer_pitch   = fbinfo.framebuffer_pitch;
        stivale_struct.fb_memory_model     = STIVALE_FBUF_MMODEL_RGB;
        stivale_struct.fb_red_mask_size    = fbinfo.red_mask_size;
        stivale_struct.fb_red_mask_shift   = fbinfo.red_mask_shift;
        stivale_struct.fb_green_mask_size  = fbinfo.green_mask_size;
        stivale_struct.fb_green_mask_shift = fbinfo.green_mask_shift;
        stivale_struct.fb_blue_mask_size   = fbinfo.blue_mask_size;
        stivale_struct.fb_blue_mask_shift  = fbinfo.blue_mask_shift;
    }

    bool want_5lv = level5pg && (stivale_hdr.flags & (1 << 1));
    pagemap_t pagemap = stivale_build_pagemap(want_5lv);

    size_t memmap_entries;
    struct e820_entry_t *memmap = get_memmap(&memmap_entries);

    stivale_struct.memory_map_entries = (uint64_t)memmap_entries;
    stivale_struct.memory_map_addr    = (uint64_t)(size_t)memmap;

#if defined (uefi)
    efi_exit_boot_services();
#endif

    stivale_spinup(bits, want_5lv, &pagemap,
                   entry_point, &stivale_struct, stivale_hdr.stack);
}

pagemap_t stivale_build_pagemap(bool level5pg) {
    pagemap_t pagemap = new_pagemap(level5pg ? 5 : 4);
    uint64_t higher_half_base = level5pg ? 0xff00000000000000 : 0xffff800000000000;

    // Map 0 to 2GiB at 0xffffffff80000000
    for (uint64_t i = 0; i < 0x80000000; i += PAGE_SIZE) {
        map_page(pagemap, 0xffffffff80000000 + i, i, 0x03);
    }

    // Map 0 to 4GiB at higher half base and 0
    for (uint64_t i = 0; i < 0x100000000; i += PAGE_SIZE) {
        map_page(pagemap, i, i, 0x03);
        map_page(pagemap, higher_half_base + i, i, 0x03);
    }

    size_t _memmap_entries = memmap_entries;
    struct e820_entry_t *_memmap =
        ext_mem_alloc(_memmap_entries * sizeof(struct e820_entry_t));
    for (size_t i = 0; i < _memmap_entries; i++)
        _memmap[i] = memmap[i];

    // Map any other region of memory from the memmap
    for (size_t i = 0; i < _memmap_entries; i++) {
        uint64_t base   = _memmap[i].base;
        uint64_t length = _memmap[i].length;
        uint64_t top    = base + length;

        uint64_t aligned_base   = ALIGN_DOWN(base, PAGE_SIZE);
        uint64_t aligned_top    = ALIGN_UP(top, PAGE_SIZE);
        uint64_t aligned_length = aligned_top - aligned_base;

        for (uint64_t i = 0; i < aligned_length; i += PAGE_SIZE) {
            uint64_t page = aligned_base + i;
            map_page(pagemap, page, page, 0x03);
            map_page(pagemap, higher_half_base + page, page, 0x03);
        }
    }

    return pagemap;
}

#if defined (uefi)
extern symbol ImageBase;
#endif

__attribute__((noreturn)) void stivale_spinup_32(
                 int bits, bool level5pg, uint32_t pagemap_top_lv,
                 uint32_t entry_point_lo, uint32_t entry_point_hi,
                 void *stivale_struct, uint32_t stack_lo, uint32_t stack_hi);

__attribute__((noreturn)) void stivale_spinup(
                 int bits, bool level5pg, pagemap_t *pagemap,
                 uint64_t entry_point, void *stivale_struct, uint64_t stack) {
    mtrr_restore();

#if defined (bios)
    if (bits == 64) {
        // If we're going 64, we might as well call this BIOS interrupt
        // to tell the BIOS that we are entering Long Mode, since it is in
        // the specification.
        struct rm_regs r = {0};
        r.eax = 0xec00;
        r.ebx = 0x02;   // Long mode only
        rm_int(0x15, &r, &r);
    }
#endif

    pic_mask_all();
    pic_flush();

#if defined (uefi)
    asm volatile (
        "lgdt %0\n\t"
        :
        : "m"(gdt)
        : "memory"
    );

    do_32(stivale_spinup_32, 8,
        bits, level5pg, (uint32_t)(uintptr_t)pagemap->top_level,
        (uint32_t)entry_point, (uint32_t)(entry_point >> 32),
        stivale_struct,
        (uint32_t)stack, (uint32_t)(stack >> 32));
#endif

#if defined (bios)
    stivale_spinup_32(bits, level5pg, (uint32_t)(uintptr_t)pagemap->top_level,
        (uint32_t)entry_point, (uint32_t)(entry_point >> 32),
        stivale_struct,
        (uint32_t)stack, (uint32_t)(stack >> 32));
#endif

    __builtin_unreachable();
}
