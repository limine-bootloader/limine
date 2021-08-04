#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <protos/stivale.h>
#include <protos/stivale2.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/acpi.h>
#include <lib/config.h>
#include <lib/time.h>
#include <lib/print.h>
#include <lib/real.h>
#include <lib/libc.h>
#include <lib/gterm.h>
#include <lib/uri.h>
#include <sys/smp.h>
#include <sys/cpu.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <lib/pe.h>
#include <sys/pic.h>
#include <sys/lapic.h>
#include <fs/file.h>
#include <mm/pmm.h>
#include <stivale/stivale2.h>
#include <pxe/tftp.h>
#include <drivers/edid.h>
#include <drivers/vga_textmode.h>

#define FILE_ELF 1
#define FILE_PE  2

#define REPORTED_ADDR(PTR) \
    ((PTR) + ((stivale2_hdr.flags & (1 << 1)) ? \
    (want_5lv ? 0xff00000000000000 : 0xffff800000000000) : 0))

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

#define append_tag(S, TAG) ({                              \
    (TAG)->next = (S)->tags;                               \
    (S)->tags   = REPORTED_ADDR((uint64_t)(uintptr_t)TAG); \
})

#if defined (__i386__)
extern symbol stivale2_term_write_entry;
void *stivale2_rt_stack = NULL;
void *stivale2_term_buf = NULL;
#endif

void stivale2_load(char *config, char *cmdline, bool pxe, void *efi_system_table) {
    struct file_handle *kernel_file = ext_mem_alloc(sizeof(struct file_handle));

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("stivale2: KERNEL_PATH not specified");

    print("stivale2: Loading kernel `%s`...\n", kernel_path);

    if (!uri_open(kernel_file, kernel_path))
        panic("stivale2: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    struct stivale2_header stivale2_hdr;

    bool level5pg = false;
    uint64_t slide = 0;
    uint64_t entry_point = 0;

    struct elf_range *ranges;
    uint64_t ranges_count;

    uint8_t *kernel = freadall(kernel_file, STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE);
    int format;
    int bits;
    if (elf_detect(kernel)) {
        format = FILE_ELF;
        bits = elf_bits(kernel);
    } else if (pe_detect(kernel)) {
        format = FILE_PE;
        bits = pe_bits(kernel);
    } else {
        format = -1;
        bits = -1;
    }

    char *kaslr_s = config_get_value(config, 0, "KASLR");
    bool kaslr = true;
    if ((kaslr_s != NULL && strcmp(kaslr_s, "no") == 0) || format == FILE_PE)
        kaslr = false;

    bool loaded_by_anchor = false;

    if (format == -1) {
        struct stivale2_anchor *anchor;
        if (!stivale_load_by_anchor((void **)&anchor, "STIVALE2 ANCHOR", kernel, kernel_file->size)) {
            panic("stivale2: Not a valid ELF, PE, or anchored file.");
        }

        bits = anchor->bits;

        memcpy(&stivale2_hdr, (void *)(uintptr_t)anchor->phys_stivale2hdr,
               sizeof(struct stivale2_header));

        loaded_by_anchor = true;
    }

    bool want_pmrs = false;

    int ret = 0;
    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
                panic("stivale2: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
                printv("stivale2: CPU has 5-level paging support\n");
                level5pg = true;
            }

            if (!loaded_by_anchor) {
                if (format == FILE_ELF) {
                    ret = elf64_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                             sizeof(struct stivale2_header), 0);
                    if (ret) {
                        goto failed_to_load_header_section;
                    }

                    if ((stivale2_hdr.flags & (1 << 2))) {
                        if (bits == 32) {
                            panic("stivale2: PMRs are not supported for 32-bit kernels");
                        } else if (loaded_by_anchor) {
                            panic("stivale2: PMRs are not supported for anchored kernels");
                        }
                        want_pmrs = true;
                    }

                    if (elf64_load(kernel, &entry_point, NULL, &slide,
                                STIVALE2_MMAP_KERNEL_AND_MODULES, kaslr, false,
                                want_pmrs ? &ranges : NULL,
                                want_pmrs ? &ranges_count : NULL))
                        panic("stivale2: ELF64 load failure");

                    ret = elf64_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                             sizeof(struct stivale2_header), slide);
                } else if (format == FILE_PE) {
                    ret = pe64_load_section(kernel, &stivale2_hdr, ".stvl2",
                                            sizeof(struct stivale2_header));

                    if (ret) {
                        goto failed_to_load_header_section;
                    }

                    if (stivale2_hdr.flags & (1 << 2))
                        panic("stivale2: PMRs are not allowed for PE kernels");

                    if (pe64_load(kernel, &entry_point, NULL,
                                  STIVALE2_MMAP_KERNEL_AND_MODULES))
                        panic("stivale2: PE64 load failure");
                } else {
                    panic("stivale2: Unknown kernel format");
                }
            }
            break;
        }
        case 32: {
            if (!loaded_by_anchor) {
                if (format == FILE_ELF) {
                    if (elf32_load(kernel, (uint32_t *)&entry_point, NULL, 10))
                        panic("stivale2: ELF32 load failure");

                    ret = elf32_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                             sizeof(struct stivale2_header));
                } else if (format == FILE_PE) {
                    if (pe32_load(kernel, (uint32_t *)&entry_point, NULL, 10))
                        panic("stivale2: PE32 load failure");

                    ret = elf32_load_section(kernel, &stivale2_hdr, ".stvl2",
                                             sizeof(struct stivale2_header));
                } else {
                    panic("stivale2: Unknown kernel format");
                }
            }

            break;
        }
        default:
            panic("stivale2: Not 32 nor 64-bit kernel. What is this?");
    }

    printv("stivale2: %u-bit kernel detected\n", bits);

failed_to_load_header_section:
    char *section_name;
    char *format_name;
    if (format == FILE_ELF) {
        section_name = ".stivale2hdr";
        format_name = "ELF";
    } else if (format == FILE_PE) {
        section_name = ".stvl2";
        format_name = "ELF";
    } else {
        panic("stivale2: Unknown kernel format");
    }
    switch (ret) {
        case 1:
            panic("stivale2: File is not a valid %s.", format_name);
        case 2:
            panic("stivale2: Section %s not found.", section_name);
        case 3:
            panic("stivale2: Section %s exceeds the size of the struct.", section_name);
        case 4:
            panic("stivale2: Section %s is smaller than size of the struct.", section_name);
    }

    if ((stivale2_hdr.flags & (1 << 1)) && bits == 32) {
        panic("stivale2: Higher half addresses header flag not supported in 32-bit mode.");
    }

    bool want_5lv = (get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_5LV_PAGING_ID) ? true : false) && level5pg;

    if (stivale2_hdr.entry_point != 0)
        entry_point = stivale2_hdr.entry_point;

    if (verbose) {
        print("stivale2: Kernel slide: %X\n", slide);

        print("stivale2: Entry point at: %X\n", entry_point);
        print("stivale2: Requested stack at: %X\n", stivale2_hdr.stack);
    }

    // The spec says the stack has to be 16-byte aligned
    if ((stivale2_hdr.stack & (16 - 1)) != 0) {
        panic("stivale2: Requested stack is not 16-byte aligned");
    }

    // It also says the stack cannot be NULL for 32-bit kernels
    if (bits == 32 && stivale2_hdr.stack == 0) {
        panic("stivale2: The stack cannot be 0 for 32-bit kernels");
    }

    strcpy(stivale2_struct.bootloader_brand, "Limine");
    strcpy(stivale2_struct.bootloader_version, LIMINE_VERSION);

    //////////////////////////////////////////////
    // Create kernel file struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_kernel_file *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_kernel_file));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_KERNEL_FILE_ID;

    tag->kernel_file = REPORTED_ADDR((uint64_t)(uintptr_t)kernel);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create kernel slide struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_kernel_slide *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_kernel_slide));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_KERNEL_SLIDE_ID;

    tag->kernel_slide = slide;

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create firmware struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_firmware *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_firmware));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_FIRMWARE_ID;

#if bios == 1
    tag->flags = 1 << 0;   // bit 0 = BIOS boot
#endif

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create modules struct tag
    //////////////////////////////////////////////
    {
    size_t module_count;
    for (module_count = 0; ; module_count++) {
        char *module_file = config_get_value(config, module_count, "MODULE_PATH");
        if (module_file == NULL)
            break;
    }

    struct stivale2_struct_tag_modules *tag =
        ext_mem_alloc(sizeof(struct stivale2_struct_tag_modules)
                    + sizeof(struct stivale2_module) * module_count);

    tag->tag.identifier = STIVALE2_STRUCT_TAG_MODULES_ID;
    tag->module_count   = module_count;

    for (size_t i = 0; i < module_count; i++) {
        char *module_path = config_get_value(config, i, "MODULE_PATH");

        struct stivale2_module *m = &tag->modules[i];

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

        print("stivale2: Loading module `%s`...\n", module_path);

        struct file_handle f;
        if (!uri_open(&f, module_path))
            panic("stivale2: Failed to open module with path `%s`. Is the path correct?", module_path);

        m->begin = REPORTED_ADDR((uint64_t)(size_t)freadall(&f, STIVALE2_MMAP_KERNEL_AND_MODULES));
        m->end   = m->begin + f.size;

        if (verbose) {
            print("stivale2: Requested module %u:\n", i);
            print("          Path:   %s\n", module_path);
            print("          String: %s\n", m->string);
            print("          Begin:  %X\n", m->begin);
            print("          End:    %X\n", m->end);
        }
    }

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create RSDP struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_rsdp *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_rsdp));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_RSDP_ID;

    uint64_t rsdp = (uint64_t)(size_t)acpi_get_rsdp();
    if (rsdp)
        tag->rsdp = REPORTED_ADDR(rsdp);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create SMBIOS struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_smbios *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_smbios));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_SMBIOS_ID;

    uint64_t smbios_entry_32 = 0, smbios_entry_64 = 0;
    acpi_get_smbios((void **)&smbios_entry_32, (void **)&smbios_entry_64);

    if (smbios_entry_32)
        tag->smbios_entry_32 = REPORTED_ADDR(smbios_entry_32);
    if (smbios_entry_64)
        tag->smbios_entry_64 = REPORTED_ADDR(smbios_entry_64);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create cmdline struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_cmdline *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_cmdline));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_CMDLINE_ID;

    tag->cmdline = REPORTED_ADDR((uint64_t)(size_t)cmdline);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create epoch struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_epoch *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_epoch));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_EPOCH_ID;

    tag->epoch = time();
    printv("stivale2: Current epoch: %U\n", tag->epoch);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create framebuffer struct tag
    //////////////////////////////////////////////
    {

    struct fb_info *fb = NULL;
    struct fb_info _fb;

    struct stivale2_header_tag_any_video *avtag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_ANY_VIDEO_ID);

    struct stivale2_header_tag_framebuffer *hdrtag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_FRAMEBUFFER_ID);

    int req_width = 0, req_height = 0, req_bpp = 0;

    if (hdrtag != NULL) {
        req_width  = hdrtag->framebuffer_width;
        req_height = hdrtag->framebuffer_height;
        req_bpp    = hdrtag->framebuffer_bpp;

        char *resolution = config_get_value(config, 0, "RESOLUTION");
        if (resolution != NULL)
            parse_resolution(&req_width, &req_height, &req_bpp, resolution);
    }

    struct stivale2_header_tag_terminal *terminal_hdr_tag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_TERMINAL_ID);

    if (bits == 64 && terminal_hdr_tag != NULL && hdrtag != NULL) {
        term_vbe(req_width, req_height);

        if (current_video_mode < 0) {
            panic("stivale2: Failed to initialise terminal");
        }

        fb = &fbinfo;

        struct stivale2_struct_tag_terminal *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_terminal));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_TERMINAL_ID;

        // We provide max allowed string length
        tag->flags |= (1 << 1);

#if defined (__i386__)
        if (stivale2_rt_stack == NULL) {
            stivale2_rt_stack = ext_mem_alloc(8192);
        }

        stivale2_term_buf = ext_mem_alloc(8192);

        tag->term_write = (uintptr_t)(void *)stivale2_term_write_entry;
        tag->max_length = 8192;
#elif defined (__x86_64__)
        tag->term_write = (uintptr_t)term_write;
        tag->max_length = 0;
#endif

        // We provide rows and cols
        tag->flags |= (1 << 0);
        tag->cols = term_cols;
        tag->rows = term_rows;

        append_tag(&stivale2_struct, (struct stivale2_tag *)tag);

        goto skip_modeset;
    } else {
        fb = &_fb;
    }

    if (hdrtag != NULL || (avtag != NULL && uefi)
    || (avtag != NULL && avtag->preference == 0)) {
        term_deinit();

        if (fb_init(fb, req_width, req_height, req_bpp)) {
skip_modeset:;
            struct stivale2_struct_tag_framebuffer *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_framebuffer));
            tag->tag.identifier = STIVALE2_STRUCT_TAG_FRAMEBUFFER_ID;

            memmap_alloc_range(fb->framebuffer_addr,
                               (uint64_t)fb->framebuffer_pitch * fb->framebuffer_height,
                               MEMMAP_FRAMEBUFFER, false, false, false, true);

            tag->memory_model       = STIVALE2_FBUF_MMODEL_RGB;
            tag->framebuffer_addr   = REPORTED_ADDR(fb->framebuffer_addr);
            tag->framebuffer_width  = fb->framebuffer_width;
            tag->framebuffer_height = fb->framebuffer_height;
            tag->framebuffer_bpp    = fb->framebuffer_bpp;
            tag->framebuffer_pitch  = fb->framebuffer_pitch;
            tag->red_mask_size      = fb->red_mask_size;
            tag->red_mask_shift     = fb->red_mask_shift;
            tag->green_mask_size    = fb->green_mask_size;
            tag->green_mask_shift   = fb->green_mask_shift;
            tag->blue_mask_size     = fb->blue_mask_size;
            tag->blue_mask_shift    = fb->blue_mask_shift;

            append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
        }
    } else {
#if uefi == 1
        panic("stivale2: Cannot use text mode with UEFI.");
#elif bios == 1
        int rows, cols;
        init_vga_textmode(&rows, &cols, false);

        struct stivale2_struct_tag_textmode *tmtag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_textmode));
        tmtag->tag.identifier = STIVALE2_STRUCT_TAG_TEXTMODE_ID;

        tmtag->address = 0xb8000;
        tmtag->rows = 25;
        tmtag->cols = 80;
        tmtag->bytes_per_char = 2;

        append_tag(&stivale2_struct, (struct stivale2_tag *)tmtag);
#endif
    }
    }

    //////////////////////////////////////////////
    // Create EDID struct tag
    //////////////////////////////////////////////
    {
    struct edid_info_struct *edid_info = get_edid_info();

    if (edid_info != NULL) {
        struct stivale2_struct_tag_edid *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_edid) + sizeof(struct edid_info_struct));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_EDID_ID;

        tag->edid_size = sizeof(struct edid_info_struct);

        memcpy(tag->edid_information, edid_info, sizeof(struct edid_info_struct));

        append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

#if bios == 1
    //////////////////////////////////////////////
    // Create PXE struct tag
    //////////////////////////////////////////////
    if (pxe) {
        struct stivale2_struct_tag_pxe_server_info *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_pxe_server_info));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_PXE_SERVER_INFO;
        tag->server_ip = get_boot_server_info();
        append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }
#else
    (void)pxe;
#endif

    //////////////////////////////////////////////
    // Create PMRs struct tag
    //////////////////////////////////////////////
    {
    if (want_pmrs) {
        struct stivale2_struct_tag_pmrs *tag =
            ext_mem_alloc(sizeof(struct stivale2_struct_tag_pmrs)
                          + ranges_count * sizeof(struct stivale2_pmr));

        tag->tag.identifier = STIVALE2_STRUCT_TAG_PMRS_ID;

        tag->entries = ranges_count;

        memcpy(tag->pmrs, ranges, ranges_count * sizeof(struct stivale2_pmr));

        append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

    //////////////////////////////////////////////
    // Create EFI system table struct tag
    //////////////////////////////////////////////
    {
    if (efi_system_table != NULL) {
        struct stivale2_struct_tag_efi_system_table *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_efi_system_table));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_EFI_SYSTEM_TABLE_ID;

        tag->system_table = REPORTED_ADDR((uint64_t)(uintptr_t)efi_system_table);

        append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

    bool unmap_null = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_UNMAP_NULL_ID) ? true : false;

    pagemap_t pagemap = {0};
    if (bits == 64)
        pagemap = stivale_build_pagemap(want_5lv, unmap_null,
                                        want_pmrs ? ranges : NULL,
                                        want_pmrs ? ranges_count : 0);

#if uefi == 1
    efi_exit_boot_services();
#endif

    //////////////////////////////////////////////
    // Create SMP struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_header_tag_smp *smp_hdr_tag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_SMP_ID);
    if (smp_hdr_tag != NULL) {
        struct stivale2_struct_tag_smp *tag;
        struct smp_information *smp_info;
        size_t cpu_count;
        uint32_t bsp_lapic_id;
        smp_info = init_smp(sizeof(struct stivale2_struct_tag_smp), (void **)&tag,
                            &cpu_count, &bsp_lapic_id,
                            bits == 64, want_5lv,
                            pagemap, smp_hdr_tag->flags & 1, want_pmrs);

        if (smp_info != NULL) {
            tag->tag.identifier = STIVALE2_STRUCT_TAG_SMP_ID;
            tag->bsp_lapic_id   = bsp_lapic_id;
            tag->cpu_count      = cpu_count;
            tag->flags         |= (smp_hdr_tag->flags & 1) && x2apic_check();

            append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
        }
    }
    }

    //////////////////////////////////////////////
    // Create memmap struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_memmap *tag =
        ext_mem_alloc(sizeof(struct stivale2_struct_tag_memmap) +
                       sizeof(struct e820_entry_t) * 256);

    // Reserve 32K at 0x70000
    memmap_alloc_range(0x70000, 0x8000, MEMMAP_USABLE, true, true, false, false);

    size_t mmap_entries;
    struct e820_entry_t *mmap = get_memmap(&mmap_entries);

    tag->tag.identifier = STIVALE2_STRUCT_TAG_MEMMAP_ID;
    tag->entries = (uint64_t)mmap_entries;

    memcpy((void*)tag + sizeof(struct stivale2_struct_tag_memmap),
           mmap, sizeof(struct e820_entry_t) * mmap_entries);

    append_tag(&stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // List tags
    //////////////////////////////////////////////
    if (verbose) {
        print("stivale2: Generated tags:\n");
        struct stivale2_tag *taglist =
                    (void*)(uintptr_t)(stivale2_struct.tags & (uint64_t)0xffffffff);
        for (size_t i = 0; ; i++) {
            print("          Tag #%u  ID: %X\n", i, taglist->identifier);
            if (taglist->next) {
                taglist = (void*)(uintptr_t)(taglist->next & (uint64_t)0xffffffff);
            } else {
                break;
            }
        }
    }

    // Clear terminal for kernels that will use the stivale2 terminal
    term_write("\e[2J\e[H", 7);

    stivale_spinup(bits, want_5lv, &pagemap, entry_point,
                   REPORTED_ADDR((uint64_t)(uintptr_t)&stivale2_struct),
                   stivale2_hdr.stack, want_pmrs);
}
