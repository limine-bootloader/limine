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
#include <sys/idt.h>
#include <sys/lapic.h>
#include <fs/file.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <stivale.h>
#include <drivers/vga_textmode.h>

#define REPORTED_ADDR(PTR) \
    ((PTR) + ((stivale_hdr.flags & (1 << 3)) ? \
    (want_5lv ? 0xff00000000000000 : 0xffff800000000000) : 0))

bool stivale_load_by_anchor(void **_anchor, const char *magic,
                            uint8_t *file, uint64_t filesize) {
    struct stivale_anchor *anchor = NULL;
    size_t magiclen = strlen(magic);
    for (size_t i = 0; i < filesize; i += 16) {
        if (memcmp(file + i, magic, magiclen) == 0) {
            anchor = (void *)(file + i);
        }
    }
    if (anchor == NULL) {
        return false;
    }

    memmap_alloc_range(anchor->phys_load_addr, filesize, MEMMAP_KERNEL_AND_MODULES,
                       true, true, false, false);
    memcpy((void *)(uintptr_t)anchor->phys_load_addr, file, filesize);

    size_t bss_size = anchor->phys_bss_end - anchor->phys_bss_start;
    memmap_alloc_range(anchor->phys_bss_start, bss_size, MEMMAP_KERNEL_AND_MODULES,
                       true, true, false, false);
    memset((void *)(uintptr_t)anchor->phys_bss_start, 0, bss_size);

    *_anchor = anchor;

    return true;
}

struct stivale_struct stivale_struct = {0};

void stivale_load(char *config, char *cmdline) {
    // BIOS or UEFI?
#if bios == 1
    stivale_struct.flags |= (1 << 0);
#endif

    stivale_struct.flags |= (1 << 1);    // we give colour information
    stivale_struct.flags |= (1 << 2);    // we give SMBIOS information

    struct file_handle *kernel_file;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("stivale: KERNEL_PATH not specified");

    print("stivale: Loading kernel `%s`...\n", kernel_path);

    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic("stivale: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    char *kaslr_s = config_get_value(config, 0, "KASLR");
    bool kaslr = true;
    if (kaslr_s != NULL && strcmp(kaslr_s, "no") == 0)
        kaslr = false;

    struct stivale_header stivale_hdr;

    bool level5pg = false;
    uint64_t slide = 0;
    uint64_t entry_point = 0;

    uint8_t *kernel = freadall(kernel_file, STIVALE_MMAP_BOOTLOADER_RECLAIMABLE);
    int bits = elf_bits(kernel);
    bool loaded_by_anchor = false;

    size_t kernel_file_size = kernel_file->size;

    fclose(kernel_file);

    if (bits == -1) {
        struct stivale_anchor *anchor;
        if (!stivale_load_by_anchor((void **)&anchor, "STIVALE1 ANCHOR", kernel, kernel_file_size)) {
            panic("stivale: Not a valid ELF or anchored file.");
        }

        bits = anchor->bits;

        memcpy(&stivale_hdr, (void *)(uintptr_t)anchor->phys_stivalehdr,
               sizeof(struct stivale_header));

        loaded_by_anchor = true;
    }

    int ret = 0;
    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
                panic("stivale: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
                printv("stivale: CPU has 5-level paging support\n");
                level5pg = true;
            }

            if (!loaded_by_anchor) {
                if (elf64_load(kernel, &entry_point, NULL, &slide,
                               STIVALE_MMAP_KERNEL_AND_MODULES, kaslr, false,
                               NULL, NULL, false, NULL, NULL))
                    panic("stivale: ELF64 load failure");

                ret = elf64_load_section(kernel, &stivale_hdr, ".stivalehdr",
                                         sizeof(struct stivale_header), slide);
            }

            break;
        }
        case 32: {
            if (!loaded_by_anchor) {
                if (elf32_load(kernel, (uint32_t *)&entry_point, NULL, 10))
                    panic("stivale: ELF32 load failure");

                ret = elf32_load_section(kernel, &stivale_hdr, ".stivalehdr",
                                         sizeof(struct stivale_header));
            }

            break;
        }
        default:
            panic("stivale: Not 32 nor 64-bit kernel. What is this?");
    }

    printv("stivale: %u-bit kernel detected\n", bits);

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

    if (verbose) {
        print("stivale: Kernel slide: %X\n", slide);

        print("stivale: Entry point at: %X\n", entry_point);
        print("stivale: Requested stack at: %X\n", stivale_hdr.stack);
    }

    // The spec says the stack has to be 16-byte aligned
    if ((stivale_hdr.stack & (16 - 1)) != 0) {
        print("stivale: WARNING: Requested stack is not 16-byte aligned\n");
    }

    // It also says the stack cannot be NULL for 32-bit kernels
    if (bits == 32 && stivale_hdr.stack == 0) {
        panic("stivale: The stack cannot be 0 for 32-bit kernels");
    }

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

        struct file_handle *f;
        if ((f = uri_open(module_path)) == NULL)
            panic("stivale: Failed to open module with path `%s`. Is the path correct?", module_path);

        m->begin = REPORTED_ADDR((uint64_t)(size_t)freadall(f, STIVALE_MMAP_KERNEL_AND_MODULES));
        m->end   = m->begin + f->size;
        m->next  = 0;

        *prev_mod_ptr = REPORTED_ADDR((uint64_t)(size_t)m);
        prev_mod_ptr  = &m->next;

        fclose(f);

        if (verbose) {
            print("stivale: Requested module %u:\n", i);
            print("         Path:   %s\n", module_path);
            print("         String: %s\n", m->string);
            print("         Begin:  %X\n", m->begin);
            print("         End:    %X\n", m->end);
        }
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
    printv("stivale: Current epoch: %U\n", stivale_struct.epoch);

    term_deinit();

    if (stivale_hdr.flags & (1 << 0)) {
        size_t req_width  = stivale_hdr.framebuffer_width;
        size_t req_height = stivale_hdr.framebuffer_height;
        size_t req_bpp    = stivale_hdr.framebuffer_bpp;

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
#if uefi == 1
        panic("stivale: Cannot use text mode with UEFI.");
#elif bios == 1
        size_t rows, cols;
        init_vga_textmode(&rows, &cols, false);
#endif
    }

#if uefi == 1
    efi_exit_boot_services();
#endif

    pagemap_t pagemap = {0};
    if (bits == 64)
        pagemap = stivale_build_pagemap(want_5lv, false, NULL, 0, false, 0, 0);

    // Reserve 32K at 0x70000 if possible
    if (!memmap_alloc_range(0x70000, 0x8000, MEMMAP_USABLE, true, false, false, false)) {
        if ((stivale_hdr.flags & (1 << 4)) == 0) {
            panic("stivale: Could not allocate low memory area");
        }
    }

    struct e820_entry_t *mmap_copy = ext_mem_alloc(256 * sizeof(struct e820_entry_t));

    size_t mmap_entries;
    struct e820_entry_t *mmap = get_memmap(&mmap_entries);

    if (mmap_entries > 256) {
        panic("stivale: Too many memory map entries!");
    }

    memcpy(mmap_copy, mmap, mmap_entries * sizeof(struct e820_entry_t));

    stivale_struct.memory_map_entries = (uint64_t)mmap_entries;
    stivale_struct.memory_map_addr    = REPORTED_ADDR((uint64_t)(size_t)mmap_copy);

    stivale_spinup(bits, want_5lv, &pagemap,
                   entry_point, REPORTED_ADDR((uint64_t)(uintptr_t)&stivale_struct),
                   stivale_hdr.stack, false);
}

pagemap_t stivale_build_pagemap(bool level5pg, bool unmap_null, struct elf_range *ranges, size_t ranges_count,
                                bool want_fully_virtual, uint64_t physical_base, uint64_t virtual_base) {
    pagemap_t pagemap = new_pagemap(level5pg ? 5 : 4);
    uint64_t higher_half_base = level5pg ? 0xff00000000000000 : 0xffff800000000000;

    if (ranges_count == 0) {
        // Map 0 to 2GiB at 0xffffffff80000000
        for (uint64_t i = 0; i < 0x80000000; i += 0x200000) {
            map_page(pagemap, 0xffffffff80000000 + i, i, 0x03, true);
        }
    } else {
        for (size_t i = 0; i < ranges_count; i++) {
            uint64_t virt = ranges[i].base;
            uint64_t phys;

            if (virt & ((uint64_t)1 << 63)) {
                if (want_fully_virtual) {
                    phys = physical_base + (virt - virtual_base);
                } else {
                    phys = virt - FIXED_HIGHER_HALF_OFFSET_64;
                }
            } else {
                panic("stivale2: Protected memory ranges are only supported for higher half kernels");
            }

            uint64_t pf = VMM_FLAG_PRESENT |
                (ranges[i].permissions & ELF_PF_X ? 0 : VMM_FLAG_NOEXEC) |
                (ranges[i].permissions & ELF_PF_W ? VMM_FLAG_WRITE : 0);

            for (uint64_t j = 0; j < ranges[i].length; j += 0x1000) {
                map_page(pagemap, virt + j, phys + j, pf, false);
            }
        }
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

        for (uint64_t j = 0; j < aligned_length; j += 0x200000) {
            uint64_t page = aligned_base + j;
            map_page(pagemap, page, page, 0x03, true);
            map_page(pagemap, higher_half_base + page, page, 0x03, true);
        }
    }

    return pagemap;
}

#if uefi == 1
extern symbol ImageBase;
#endif

__attribute__((noreturn)) void stivale_spinup_32(
                 int bits, bool level5pg, uint32_t pagemap_top_lv,
                 uint32_t entry_point_lo, uint32_t entry_point_hi,
                 uint32_t stivale_struct_lo, uint32_t stivale_struct_hi,
                 uint32_t stack_lo, uint32_t stack_hi);

__attribute__((noreturn)) void stivale_spinup(
                 int bits, bool level5pg, pagemap_t *pagemap,
                 uint64_t entry_point, uint64_t _stivale_struct, uint64_t stack,
                 bool enable_nx) {
#if bios == 1
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

    if (enable_nx) {
        vmm_assert_nx();
    }

    pic_mask_all();
    io_apic_mask_all();

    irq_flush_type = IRQ_PIC_APIC_FLUSH;

    common_spinup(stivale_spinup_32, 10,
        bits, level5pg, enable_nx, (uint32_t)(uintptr_t)pagemap->top_level,
        (uint32_t)entry_point, (uint32_t)(entry_point >> 32),
        (uint32_t)_stivale_struct, (uint32_t)(_stivale_struct >> 32),
        (uint32_t)stack, (uint32_t)(stack >> 32));
}
