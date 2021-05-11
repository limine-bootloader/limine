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
#include <stivale/stivale.h>
#include <drivers/vga_textmode.h>

#define REPORTED_ADDR(PTR) \
    ((PTR) + ((stivale_hdr.flags & (1 << 3)) ? \
    (want_5lv ? 0xff00000000000000 : 0xffff800000000000) : 0))

struct stivale_struct stivale_struct = {0};

void stivale_load(char *config, char *cmdline) {
    // BIOS or UEFI?
#if defined (bios)
    stivale_struct.flags |= (1 << 0);
#endif

    stivale_struct.flags |= (1 << 1);    // we give colour information
    stivale_struct.flags |= (1 << 2);    // we give SMBIOS information

    struct file_handle *kernel_file = ext_mem_alloc(sizeof(struct file_handle));

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("KERNEL_PATH not specified");

    if (!uri_open(kernel_file, kernel_path))
        panic("Could not open kernel resource");

    struct stivale_header stivale_hdr;

    uint8_t *kernel = freadall(kernel_file, STIVALE_MMAP_BOOTLOADER_RECLAIMABLE);

    int bits = elf_bits(kernel);

    int ret;

    bool level5pg = false;

    char *kaslr_s = config_get_value(config, 0, "KASLR");
    bool kaslr = true;
    if (kaslr_s != NULL && strcmp(kaslr_s, "no") == 0)
        kaslr = false;

    uint64_t slide = 0;
    uint64_t entry_point = 0;

    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
                panic("stivale: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
                level5pg = true;
            }

            if (elf64_load(kernel, &entry_point, &slide, STIVALE_MMAP_KERNEL_AND_MODULES, kaslr))
                panic("stivale: ELF64 load failure");

            ret = elf64_load_section(kernel, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header), slide);

            break;
        }
        case 32: {
            if (elf32_load(kernel, (uint32_t *)&entry_point, 10))
                panic("stivale: ELF32 load failure");

            ret = elf32_load_section(kernel, &stivale_hdr, ".stivalehdr", sizeof(struct stivale_header));

            break;
        }
        default:
            panic("stivale: Not 32 nor 64 bit x86 ELF file.");
    }

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

    if ((stivale_hdr.flags & (1 << 3)) && bits == 32) {
        panic("stivale: Higher half addresses header flag not supported in 32-bit mode.");
    }

    bool want_5lv = level5pg && (stivale_hdr.flags & (1 << 1));

    if (stivale_hdr.entry_point != 0)
        entry_point = stivale_hdr.entry_point;

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

        m->begin = REPORTED_ADDR((uint64_t)(size_t)freadall(&f, STIVALE_MMAP_KERNEL_AND_MODULES));
        m->end   = m->begin + f.size;
        m->next  = 0;

        *prev_mod_ptr = REPORTED_ADDR((uint64_t)(size_t)m);
        prev_mod_ptr  = &m->next;
    }

    uint64_t rsdp = (uint64_t)(size_t)acpi_get_rsdp();

    if (rsdp)
        stivale_struct.rsdp = REPORTED_ADDR(rsdp);

    uint64_t smbios_entry_32 = 0, smbios_entry_64 = 0;
    acpi_get_smbios((void **)&smbios_entry_32, (void **)&smbios_entry_64);

    if (smbios_entry_32)
        stivale_struct.smbios_entry_32 = REPORTED_ADDR(smbios_entry_32);
    if (smbios_entry_64)
        stivale_struct.smbios_entry_64 = REPORTED_ADDR(smbios_entry_64);

    stivale_struct.cmdline = REPORTED_ADDR((uint64_t)(size_t)cmdline);

    stivale_struct.epoch = time();

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

        memmap_alloc_range(fbinfo.framebuffer_addr,
                           (uint64_t)fbinfo.framebuffer_pitch * fbinfo.framebuffer_height,
                           MEMMAP_FRAMEBUFFER, false, false, false, true);

        stivale_struct.framebuffer_addr    = REPORTED_ADDR((uint64_t)fbinfo.framebuffer_addr);
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
    } else {
#if defined (uefi)
        panic("stivale: Cannot use text mode with UEFI.");
#elif defined (bios)
        int rows, cols;
        init_vga_textmode(&rows, &cols, false);
#endif
    }

#if defined (uefi)
    efi_exit_boot_services();
#endif

    pagemap_t pagemap = {0};
    if (bits == 64)
        pagemap = stivale_build_pagemap(want_5lv, false);

    // Reserve 32K at 0x70000
    memmap_alloc_range(0x70000, 0x8000, MEMMAP_USABLE, true, true, false, false);

    size_t memmap_entries;
    struct e820_entry_t *memmap = get_memmap(&memmap_entries);

    stivale_struct.memory_map_entries = (uint64_t)memmap_entries;
    stivale_struct.memory_map_addr    = REPORTED_ADDR((uint64_t)(size_t)memmap);

    stivale_spinup(bits, want_5lv, &pagemap,
                   entry_point, REPORTED_ADDR((uint64_t)(uintptr_t)&stivale_struct),
                   stivale_hdr.stack);
}

pagemap_t stivale_build_pagemap(bool level5pg, bool unmap_null) {
    pagemap_t pagemap = new_pagemap(level5pg ? 5 : 4);
    uint64_t higher_half_base = level5pg ? 0xff00000000000000 : 0xffff800000000000;

    // Map 0 to 2GiB at 0xffffffff80000000
    for (uint64_t i = 0; i < 0x80000000; i += 0x200000) {
        map_page(pagemap, 0xffffffff80000000 + i, i, 0x03, true);
    }

    // Sub 2MiB mappings
    for (uint64_t i = 0; i < 0x200000; i += 0x1000) {
        if (!(i == 0 && unmap_null))
            map_page(pagemap, i, i, 0x03, false);
        map_page(pagemap, higher_half_base + i, i, 0x03, false);
    }

    // Map 2MiB to 4GiB at higher half base and 0
    for (uint64_t i = 0x200000; i < 0x100000000; i += 0x200000) {
        map_page(pagemap, i, i, 0x03, true);
        map_page(pagemap, higher_half_base + i, i, 0x03, true);
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

        if (base < 0x100000000)
            base = 0x100000000;

        if (base >= top)
            continue;

        uint64_t aligned_base   = ALIGN_DOWN(base, 0x200000);
        uint64_t aligned_top    = ALIGN_UP(top, 0x200000);
        uint64_t aligned_length = aligned_top - aligned_base;

        for (uint64_t i = 0; i < aligned_length; i += 0x200000) {
            uint64_t page = aligned_base + i;
            map_page(pagemap, page, page, 0x03, true);
            map_page(pagemap, higher_half_base + page, page, 0x03, true);
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
                 uint32_t stivale_struct_lo, uint32_t stivale_struct_hi,
                 uint32_t stack_lo, uint32_t stack_hi);

__attribute__((noreturn)) void stivale_spinup(
                 int bits, bool level5pg, pagemap_t *pagemap,
                 uint64_t entry_point, uint64_t stivale_struct, uint64_t stack) {
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
    do_32(stivale_spinup_32, 9,
        bits, level5pg, (uint32_t)(uintptr_t)pagemap->top_level,
        (uint32_t)entry_point, (uint32_t)(entry_point >> 32),
        (uint32_t)stivale_struct, (uint32_t)(stivale_struct >> 32),
        (uint32_t)stack, (uint32_t)(stack >> 32));
#endif

#if defined (bios)
    stivale_spinup_32(bits, level5pg, (uint32_t)(uintptr_t)pagemap->top_level,
        (uint32_t)entry_point, (uint32_t)(entry_point >> 32),
        (uint32_t)stivale_struct, (uint32_t)(stivale_struct >> 32),
        (uint32_t)stack, (uint32_t)(stack >> 32));
#endif

    __builtin_unreachable();
}
