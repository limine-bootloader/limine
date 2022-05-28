#include <stdint.h>
#include <stddef.h>
#include <stdnoreturn.h>
#include <config.h>
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
#include <sys/idt.h>
#include <fs/file.h>
#include <mm/vmm.h>
#include <mm/pmm.h>
#include <drivers/vga_textmode.h>

noreturn void multiboot1_spinup_32(uint32_t entry_point, uint32_t multiboot1_info);

bool multiboot1_load(char *config, char *cmdline) {
    struct file_handle *kernel_file;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "multiboot1: KERNEL_PATH not specified");

    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic(true, "multiboot1: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_KERNEL_AND_MODULES);

    size_t kernel_file_size = kernel_file->size;

    fclose(kernel_file);

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

    if (header.magic != MULTIBOOT1_HEADER_MAGIC) {
        pmm_free(kernel_file, kernel_file_size);
        return false;
    }

    print("multiboot1: Loading kernel `%s`...\n", kernel_path);

    struct multiboot1_info *multiboot1_info = conv_mem_alloc(sizeof(struct multiboot1_info));

    if (header.magic + header.flags + header.checksum)
        panic(true, "multiboot1: Header checksum is invalid");

    uint32_t entry_point;
    uint32_t kernel_top;

    if (header.flags & (1 << 16)) {
        if (header.load_addr > header.header_addr)
            panic(true, "multiboot1: Illegal load address");

        size_t load_size = 0;

        if (header.load_end_addr)
            load_size = header.load_end_addr - header.load_addr;
        else
            load_size = kernel_file_size;

        memmap_alloc_range(header.load_addr, load_size, MEMMAP_KERNEL_AND_MODULES, true, true, false, false);
        memcpy((void *)(uintptr_t)header.load_addr, kernel + (header_offset
                - (header.header_addr - header.load_addr)), load_size);

        kernel_top = header.load_addr + load_size;

        if (header.bss_end_addr) {
            uintptr_t bss_addr = header.load_addr + load_size;
            if (header.bss_end_addr < bss_addr)
                panic(true, "multiboot1: Illegal bss end address");

            uint32_t bss_size = header.bss_end_addr - bss_addr;

            memmap_alloc_range(bss_addr, bss_size, MEMMAP_KERNEL_AND_MODULES, true, true, false, false);
            memset((void *)bss_addr, 0, bss_size);

            kernel_top = bss_addr + bss_size;
        }

        entry_point = header.entry_addr;
    } else {
        int bits = elf_bits(kernel);

        switch (bits) {
            case 32:
                if (elf32_load(kernel, &entry_point, &kernel_top, MEMMAP_KERNEL_AND_MODULES))
                    panic(true, "multiboot1: ELF32 load failure");
                break;
            case 64: {
                uint64_t e, t;
                if (elf64_load(kernel, &e, &t, NULL, MEMMAP_KERNEL_AND_MODULES, false, true, NULL, NULL, false, NULL, NULL, NULL, NULL))
                    panic(true, "multiboot1: ELF64 load failure");
                entry_point = e;
                kernel_top = t;

                break;
            }
            default:
                panic(true, "multiboot1: Invalid ELF file bitness");
        }
    }

    uint32_t n_modules;

    for (n_modules = 0; ; n_modules++) {
        if (config_get_value(config, n_modules, "MODULE_PATH") == NULL)
            break;
    }

    if (n_modules) {
        struct multiboot1_module *mods = conv_mem_alloc(sizeof(*mods) * n_modules);

        multiboot1_info->mods_count = n_modules;
        multiboot1_info->mods_addr = (uint32_t)(size_t)mods;

        for (size_t i = 0; i < n_modules; i++) {
            struct multiboot1_module *m = mods + i;

            struct conf_tuple conf_tuple = config_get_tuple(config, i, "MODULE_PATH", "MODULE_STRING");
            char *module_path = conf_tuple.value1;
            if (module_path == NULL)
                panic(true, "multiboot1: Module disappeared unexpectedly");

            print("multiboot1: Loading module `%s`...\n", module_path);

            struct file_handle *f;
            if ((f = uri_open(module_path)) == NULL)
                panic(true, "multiboot1: Failed to open module with path `%s`. Is the path correct?", module_path);

            char *module_cmdline = conf_tuple.value2;
            if (module_cmdline == NULL) {
                module_cmdline = "";
            }
            char *lowmem_modstr = conv_mem_alloc(strlen(module_cmdline) + 1);
            strcpy(lowmem_modstr, module_cmdline);

            void *module_addr = (void *)(uintptr_t)ALIGN_UP(kernel_top, 4096);
            while (!memmap_alloc_range((uintptr_t)module_addr, f->size, MEMMAP_KERNEL_AND_MODULES,
                                       true, false, false, false)) {
                module_addr += 0x200000;
            }
            kernel_top = (uintptr_t)module_addr + f->size;
            fread(f, module_addr, 0, f->size);

            m->begin   = (uint32_t)(size_t)module_addr;
            m->end     = m->begin + f->size;
            m->cmdline = (uint32_t)(size_t)lowmem_modstr;
            m->pad     = 0;

            fclose(f);

            if (verbose) {
                print("multiboot1: Requested module %u:\n", i);
                print("            Path:   %s\n", module_path);
                print("            String: \"%s\"\n", module_cmdline ?: "");
                print("            Begin:  %x\n", m->begin);
                print("            End:    %x\n", m->end);
            }
        }

        multiboot1_info->flags |= (1 << 3);
    }

    char *lowmem_cmdline = conv_mem_alloc(strlen(cmdline) + 1);
    strcpy(lowmem_cmdline, cmdline);
    multiboot1_info->cmdline = (uint32_t)(size_t)lowmem_cmdline;
    if (cmdline)
        multiboot1_info->flags |= (1 << 2);

    char *bootload_name = "Limine " LIMINE_VERSION;
    char *lowmem_bootname = conv_mem_alloc(strlen(bootload_name) + 1);
    strcpy(lowmem_bootname, bootload_name);

    multiboot1_info->bootloader_name = (uint32_t)(size_t)lowmem_bootname;
    multiboot1_info->flags |= (1 << 9);

    term_deinit();

    if (header.flags & (1 << 2)) {
        size_t req_width  = header.fb_width;
        size_t req_height = header.fb_height;
        size_t req_bpp    = header.fb_bpp;

        if (header.fb_mode == 0) {
            char *resolution = config_get_value(config, 0, "RESOLUTION");
            if (resolution != NULL)
                parse_resolution(&req_width, &req_height, &req_bpp, resolution);

            struct fb_info fbinfo;
            if (!fb_init(&fbinfo, req_width, req_height, req_bpp)) {
                goto nofb;
            }

            multiboot1_info->fb_addr    = (uint64_t)fbinfo.framebuffer_addr;
            multiboot1_info->fb_width   = fbinfo.framebuffer_width;
            multiboot1_info->fb_height  = fbinfo.framebuffer_height;
            multiboot1_info->fb_bpp     = fbinfo.framebuffer_bpp;
            multiboot1_info->fb_pitch   = fbinfo.framebuffer_pitch;
            multiboot1_info->fb_type    = 1;
            multiboot1_info->fb_red_mask_size    = fbinfo.red_mask_size;
            multiboot1_info->fb_red_mask_shift   = fbinfo.red_mask_shift;
            multiboot1_info->fb_green_mask_size  = fbinfo.green_mask_size;
            multiboot1_info->fb_green_mask_shift = fbinfo.green_mask_shift;
            multiboot1_info->fb_blue_mask_size   = fbinfo.blue_mask_size;
            multiboot1_info->fb_blue_mask_shift  = fbinfo.blue_mask_shift;
        } else if (header.fb_mode == 1) {
nofb:;
#if uefi == 1
            panic(true, "multiboot1: Cannot use text mode with UEFI.");
#elif bios == 1
            size_t rows, cols;
            init_vga_textmode(&rows, &cols, false);

            multiboot1_info->fb_addr    = 0xb8000;
            multiboot1_info->fb_width   = cols;
            multiboot1_info->fb_height  = rows;
            multiboot1_info->fb_bpp     = 16;
            multiboot1_info->fb_pitch   = 2 * cols;
            multiboot1_info->fb_type    = 2;
#endif
        } else {
            panic(true, "multiboot1: Illegal framebuffer type requested");
        }

        multiboot1_info->flags |= (1 << 12);
    } else {
#if uefi == 1
        panic(true, "multiboot1: Cannot use text mode with UEFI.");
#elif bios == 1
        size_t rows, cols;
        init_vga_textmode(&rows, &cols, false);
#endif
    }

#if uefi == 1
    efi_exit_boot_services();
#endif

    size_t mb_mmap_count;
    struct e820_entry_t *raw_memmap = get_raw_memmap(&mb_mmap_count);

    size_t mb_mmap_len = mb_mmap_count * sizeof(struct multiboot1_mmap_entry);
    struct multiboot1_mmap_entry *mmap = conv_mem_alloc(mb_mmap_len);

    // Multiboot is bad and passes raw memmap. We do the same to support it.
    for (size_t i = 0; i < mb_mmap_count; i++) {
        mmap[i].size = sizeof(struct multiboot1_mmap_entry) - 4;
        mmap[i].addr = raw_memmap[i].base;
        mmap[i].len  = raw_memmap[i].length;
        mmap[i].type = raw_memmap[i].type;
    }

    {
        struct meminfo memory_info = mmap_get_info(mb_mmap_count, raw_memmap);

        // Convert the uppermem and lowermem fields from bytes to
        // KiB.
        multiboot1_info->mem_lower = memory_info.lowermem / 1024;
        multiboot1_info->mem_upper = memory_info.uppermem / 1024;
    }

    multiboot1_info->mmap_length = mb_mmap_len;
    multiboot1_info->mmap_addr = ((uint32_t)(size_t)mmap);
    multiboot1_info->flags |= (1 << 0) | (1 << 6);

    irq_flush_type = IRQ_PIC_ONLY_FLUSH;

    common_spinup(multiboot1_spinup_32, 2,
                  entry_point, (uint32_t)(uintptr_t)multiboot1_info);
}
