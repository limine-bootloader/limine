#include <stdint.h>
#include <stddef.h>
#include <protos/multiboot1.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <sys/pic.h>
#include <sys/cpu.h>
#include <fs/file.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <drivers/vga_textmode.h>

struct multiboot1_info multiboot1_info = {0};

void multiboot1_load(char *config, char *cmdline) {
    struct file_handle *kernel_file = ext_mem_alloc(sizeof(*kernel_file));

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("multiboot1: KERNEL_PATH not specified");

    print("multiboot1: Loading kernel `%s`...\n", kernel_path);

    if (!uri_open(kernel_file, kernel_path))
        panic("multiboot1: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_USABLE);

    struct multiboot1_header header = {0};
    size_t header_offset = 0;

    for (header_offset = 0; header_offset < 8192; header_offset += 4) {
        uint32_t v;
        memcpy(&v, kernel + header_offset, 4);

        if (v == MULTIBOOT1_HEADER_MAGIC) {
            memcpy(&header, kernel + header_offset, sizeof(header));
            break;
        }
    }

    if (header.magic != MULTIBOOT1_HEADER_MAGIC)
        panic("multiboot1: Could not find header");

    if (header.magic + header.flags + header.checksum)
        panic("multiboot1: Header checksum is invalid");

    uint32_t entry_point = 0;

    if (header.flags & (1 << 16)) {
        if (header.load_addr > header.header_addr)
            panic("multiboot1: Illegal load address");

        size_t load_size = 0;

        if (header.load_end_addr)
            load_size = header.load_end_addr - header.load_addr;
        else
            load_size = kernel_file->size;

        memmap_alloc_range(header.load_addr, load_size, MEMMAP_KERNEL_AND_MODULES, true, true, false, false);
        memcpy((void *)(uintptr_t)header.load_addr, kernel + (header_offset
                - (header.header_addr - header.load_addr)), load_size);

        if (header.bss_end_addr) {
            uintptr_t bss_addr = header.load_addr + load_size;
            if (header.bss_end_addr < bss_addr)
                panic("multiboot1: Illegal bss end address");

            uint32_t bss_size = header.bss_end_addr - bss_addr;

            memmap_alloc_range(bss_addr, bss_size, MEMMAP_KERNEL_AND_MODULES, true, true, false, false);
            memset((void *)bss_addr, 0, bss_size);
        }

        entry_point = header.entry_addr;
    } else {
        int bits = elf_bits(kernel);

        switch (bits) {
            case 32:
                if (elf32_load(kernel, &entry_point, MEMMAP_KERNEL_AND_MODULES))
                    panic("multiboot1: ELF32 load failure");
                break;
            case 64: {
                uint64_t e;
                if (elf64_load(kernel, &e, NULL, MEMMAP_KERNEL_AND_MODULES, false, true))
                    panic("multiboot1: ELF64 load failure");
                entry_point = e;

                break;
            }
            default:
                panic("multiboot1: Invalid ELF file bitness");
        }
    }

    uint32_t n_modules;

    for (n_modules = 0; ; n_modules++) {
        if (config_get_value(config, n_modules, "MODULE_PATH") == NULL)
            break;
    }

    if (n_modules) {
        struct multiboot1_module *mods = ext_mem_alloc(sizeof(*mods) * n_modules);

        multiboot1_info.mods_count = n_modules;
        multiboot1_info.mods_addr = (uint32_t)(size_t)mods;

        for (size_t i = 0; i < n_modules; i++) {
            struct multiboot1_module *m = mods + i;

            char *module_path = config_get_value(config, i, "MODULE_PATH");
            if (module_path == NULL)
                panic("multiboot1: Module disappeared unexpectedly");

            print("multiboot1: Loading module `%s`...\n", module_path);

            struct file_handle f;
            if (!uri_open(&f, module_path))
                panic("multiboot1: Failed to open module with path `%s`. Is the path correct?", module_path);

            char *cmdline = config_get_value(config, i, "MODULE_STRING");

            m->begin   = (uint32_t)(size_t)freadall(&f, MEMMAP_KERNEL_AND_MODULES);
            m->end     = m->begin + f.size;
            m->cmdline = (uint32_t)(size_t)cmdline;
            m->pad     = 0;

            if (verbose) {
                print("multiboot1: Requested module %u:\n", i);
                print("            Path:   %s\n", module_path);
                print("            String: \"%s\"\n", cmdline ?: "");
                print("            Begin:  %x\n", m->begin);
                print("            End:    %x\n", m->end);
            }
        }

        multiboot1_info.flags |= (1 << 3);
    }

    multiboot1_info.cmdline = (uint32_t)(size_t)cmdline;
    if (cmdline)
        multiboot1_info.flags |= (1 << 2);

    multiboot1_info.bootloader_name = (uint32_t)(size_t)"Limine";
    multiboot1_info.flags |= (1 << 9);

    term_deinit();

    if (header.flags & (1 << 2)) {
        int req_width  = header.fb_width;
        int req_height = header.fb_height;
        int req_bpp    = header.fb_bpp;

        if (header.fb_mode == 0) {
            char *resolution = config_get_value(config, 0, "RESOLUTION");
            if (resolution != NULL)
                parse_resolution(&req_width, &req_height, &req_bpp, resolution);

            struct fb_info fbinfo;
            if (!fb_init(&fbinfo, req_width, req_height, req_bpp))
                panic("multiboot1: Unable to set video mode");

            multiboot1_info.fb_addr    = (uint64_t)fbinfo.framebuffer_addr;
            multiboot1_info.fb_width   = fbinfo.framebuffer_width;
            multiboot1_info.fb_height  = fbinfo.framebuffer_height;
            multiboot1_info.fb_bpp     = fbinfo.framebuffer_bpp;
            multiboot1_info.fb_pitch   = fbinfo.framebuffer_pitch;
            multiboot1_info.fb_type    = 1;
            multiboot1_info.fb_red_mask_size    = fbinfo.red_mask_size;
            multiboot1_info.fb_red_mask_shift   = fbinfo.red_mask_shift;
            multiboot1_info.fb_green_mask_size  = fbinfo.green_mask_size;
            multiboot1_info.fb_green_mask_shift = fbinfo.green_mask_shift;
            multiboot1_info.fb_blue_mask_size   = fbinfo.blue_mask_size;
            multiboot1_info.fb_blue_mask_shift  = fbinfo.blue_mask_shift;
        } else if (header.fb_mode == 1) {
#if defined (uefi)
            panic("multiboot1: Cannot use text mode with UEFI.");
#elif defined (bios)
            int rows, cols;
            init_vga_textmode(&rows, &cols, false);

            multiboot1_info.fb_addr    = 0xB8000;
            multiboot1_info.fb_width   = cols;
            multiboot1_info.fb_height  = rows;
            multiboot1_info.fb_bpp     = 16;
            multiboot1_info.fb_pitch   = 2 * cols;
            multiboot1_info.fb_type    = 2;
#endif
        } else {
            panic("multiboot1: Illegal framebuffer type requested");
        }

        multiboot1_info.flags |= (1 << 12);
    }

#if defined (uefi)
    efi_exit_boot_services();
#endif

    size_t memmap_entries;
    struct e820_entry_t *memmap = get_memmap(&memmap_entries);

    // The layouts of the e820_entry_t and multiboot1_mmap_entry structs match almost perfectly
    // apart from the padding/size being in the wrong place (at the end and beginning respectively).
    // To be able to use the memmap directly, we offset it back by 4 so the fields align properly.
    // Since we're about to exit we don't really care about what we've clobbered by doing this.
    struct multiboot1_mmap_entry *mmap = (void *)((size_t)memmap - 4);

    size_t memory_lower = 0, memory_upper = 0;

    for (size_t i = 0; i < memmap_entries; i++ ){
        mmap[i].size = sizeof(*mmap) - 4;

        if (memmap[i].type == MEMMAP_BOOTLOADER_RECLAIMABLE
                || memmap[i].type == MEMMAP_KERNEL_AND_MODULES)
            memmap[i].type = MEMMAP_USABLE;

        if (memmap[i].type == MEMMAP_USABLE) {
            if (memmap[i].base < 0x100000)
                memory_lower += memmap[i].length;
            else
                memory_upper += memmap[i].length;
        }
    }

    multiboot1_info.mem_lower = memory_lower / 1024;
    multiboot1_info.mem_upper = memory_upper / 1024;

    multiboot1_info.mmap_length = sizeof(*mmap) * memmap_entries;
    multiboot1_info.mmap_addr = ((uint32_t)(size_t)mmap);
    multiboot1_info.flags |= (1 << 0) | (1 << 6);

    multiboot1_spinup(entry_point, (uint32_t)(uintptr_t)&multiboot1_info);
}

__attribute__((noreturn)) void multiboot1_spinup_32(
                 uint32_t entry_point,
                 uint32_t multiboot1_info);

__attribute__((noreturn)) void multiboot1_spinup(
                 uint32_t entry_point, uint32_t multiboot1_info) {
    pic_mask_all();
    pic_flush();

#if defined (uefi)
    do_32(multiboot1_spinup_32, 2, entry_point, multiboot1_info);
#endif

#if defined (bios)
    multiboot1_spinup_32(entry_point, multiboot1_info);
#endif

    __builtin_unreachable();
}
