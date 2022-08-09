#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <config.h>
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
#include <sys/gdt.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <sys/pic.h>
#include <sys/lapic.h>
#include <fs/file.h>
#include <mm/pmm.h>
#include <stivale2.h>
#include <pxe/tftp.h>
#include <drivers/edid.h>
#include <drivers/vga_textmode.h>
#include <drivers/gop.h>
#include <lib/rand.h>

#define REPORTED_ADDR(PTR) \
    ((PTR) + ((stivale2_hdr.flags & (1 << 1)) ? \
    direct_map_offset : 0))

#define get_phys_addr(addr) ({ \
    uintptr_t r1; \
    if ((addr) & ((uint64_t)1 << 63)) { \
        if (want_fully_virtual) { \
            r1 = physical_base + ((addr) - virtual_base); \
        } else { \
            r1 = (addr) - FIXED_HIGHER_HALF_OFFSET_64; \
        } \
    } else { \
        r1 = addr; \
    } \
    r1; \
})

#define get_tag(s, id) ({ \
    void *r; \
    struct stivale2_tag *tag = (void *)get_phys_addr((s)->tags); \
    for (;;) { \
        if (tag == NULL) { \
            r = NULL; \
            break; \
        } \
        if (tag->identifier == (id)) { \
            r = tag; \
            break; \
        } \
        tag = (void *)get_phys_addr(tag->next); \
    } \
    r; \
})

#define append_tag(S, TAG) ({                              \
    (TAG)->next = (S)->tags;                               \
    (S)->tags   = REPORTED_ADDR((uint64_t)(uintptr_t)TAG); \
})

#if defined (__i386__)
extern symbol stivale2_term_write_entry;
void *stivale2_rt_stack = NULL;
uint64_t stivale2_term_callback_ptr = 0;
uint64_t stivale2_term_write_ptr = 0;
void stivale2_term_callback(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
#endif

noreturn void stivale2_load(char *config, char *cmdline) {
    struct stivale2_struct *stivale2_struct = ext_mem_alloc(sizeof(struct stivale2_struct));

    struct file_handle *kernel_file;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "stivale2: KERNEL_PATH not specified");

    print("stivale2: Loading kernel `%s`...\n", kernel_path);

    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic(true, "stivale2: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    char *kaslr_s = config_get_value(config, 0, "KASLR");
    bool kaslr = true;
    if (kaslr_s != NULL && strcmp(kaslr_s, "no") == 0)
        kaslr = false;

    struct stivale2_header stivale2_hdr;

    bool level5pg = false;
    uint64_t slide = 0;
    uint64_t entry_point = 0;

    struct elf_range *ranges;
    uint64_t ranges_count;

    uint8_t *kernel = freadall(kernel_file, STIVALE2_MMAP_BOOTLOADER_RECLAIMABLE);
    int bits = elf_bits(kernel);
    bool loaded_by_anchor = false;

    size_t kernel_file_size = kernel_file->size;

    struct volume *kernel_volume = kernel_file->vol;

    fclose(kernel_file);

    if (bits == -1) {
        struct stivale2_anchor *anchor;
        if (!stivale_load_by_anchor((void **)&anchor, "STIVALE2 ANCHOR", kernel, kernel_file_size)) {
            panic(true, "stivale2: Failed to load kernel by anchor");
        }

        bits = anchor->bits;

        memcpy(&stivale2_hdr, (void *)(uintptr_t)anchor->phys_stivale2hdr,
               sizeof(struct stivale2_header));

        loaded_by_anchor = true;
    } else {
        switch (bits) {
            case 64:
                if (elf64_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                       sizeof(struct stivale2_header), slide)) {
                    panic(true, "stivale2: Failed to load .stivale2hdr section");
                }
                break;
            case 32:
                if (elf32_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                       sizeof(struct stivale2_header))) {
                    panic(true, "stivale2: Failed to load .stivale2hdr section");
                }
                break;
        }
    }

    bool want_pmrs = false;
    bool want_fully_virtual = false;

    uint64_t physical_base, virtual_base;

    int ret = 0;
    switch (bits) {
        case 64: {
            // Check if 64 bit CPU
            uint32_t eax, ebx, ecx, edx;
            if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
                panic(true, "stivale2: This CPU does not support 64-bit mode.");
            }
            // Check if 5-level paging is available
            if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
                printv("stivale2: CPU has 5-level paging support\n");
                level5pg = true;
            }

            if (loaded_by_anchor && (stivale2_hdr.flags & (1 << 2))) {
                panic(true, "stivale2: PMRs are not supported for anchored kernels");
            }

            if (!loaded_by_anchor) {
                ret = elf64_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                         sizeof(struct stivale2_header), 0);
                if (ret) {
                    goto failed_to_load_header_section;
                }

                if ((stivale2_hdr.flags & (1 << 2))) {
                    if (bits == 32) {
                        panic(true, "stivale2: PMRs are not supported for 32-bit kernels");
                    }
                    want_pmrs = true;
                }

                if (want_pmrs && (stivale2_hdr.flags & (1 << 3))) {
                    want_fully_virtual = true;
                }

                if (elf64_load(kernel, &entry_point, NULL, &slide,
                               STIVALE2_MMAP_KERNEL_AND_MODULES, kaslr,
                               want_pmrs ? &ranges : NULL,
                               want_pmrs ? &ranges_count : NULL,
                               want_fully_virtual, &physical_base, &virtual_base,
                               NULL, NULL))
                    panic(true, "stivale2: ELF64 load failure");

                if (want_fully_virtual) {
                    printv("stivale2: Physical base: %X\n", physical_base);
                    printv("stivale2: Virtual base:  %X\n", virtual_base);
                }

                ret = elf64_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                         sizeof(struct stivale2_header), slide);
            }

            break;
        }
        case 32: {
            if (!loaded_by_anchor) {
                if (elf32_load(kernel, (uint32_t *)&entry_point, NULL, STIVALE2_MMAP_KERNEL_AND_MODULES))
                    panic(true, "stivale2: ELF32 load failure");

                ret = elf32_load_section(kernel, &stivale2_hdr, ".stivale2hdr",
                                         sizeof(struct stivale2_header));
            }

            break;
        }
        default:
            panic(true, "stivale2: Not 32 nor 64-bit kernel. What is this?");
    }

    printv("stivale2: %u-bit kernel detected\n", bits);

failed_to_load_header_section:
    switch (ret) {
        case 1:
            panic(true, "stivale2: File is not a valid ELF.");
        case 2:
            panic(true, "stivale2: Section .stivale2hdr not found.");
        case 3:
            panic(true, "stivale2: Section .stivale2hdr exceeds the size of the struct.");
        case 4:
            panic(true, "stivale2: Section .stivale2hdr is smaller than size of the struct.");
    }

    if ((stivale2_hdr.flags & (1 << 1)) && bits == 32) {
        panic(true, "stivale2: Higher half addresses header flag not supported in 32-bit mode.");
    }

    bool want_5lv = (get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_5LV_PAGING_ID) ? true : false) && level5pg;

    uint64_t direct_map_offset = want_5lv ? 0xff00000000000000 : 0xffff800000000000;

    {
        struct stivale2_header_tag_slide_hhdm *slt = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_SLIDE_HHDM_ID);
        if (slt != NULL) {
            if (slt->alignment % 0x200000 != 0 || slt->alignment == 0) {
                panic(true, "stivale2: Requested HHDM slide alignment is not a multiple of 2MiB");
            }

            // XXX: Assert that slt->alignment is not larger than 1GiB and ignore the value altogether.
            //      This is required for 1GiB pages.
            if (((uint64_t)0x40000000 % slt->alignment) != 0) {
                panic(true, "stivale2: 1 GiB is not a multiple of HHDM slide alignment");
            }

            direct_map_offset += (rand64() & ~((uint64_t)0x40000000 - 1)) & 0xfffffffffff;
        }
    }

    struct gdtr *local_gdt = ext_mem_alloc(sizeof(struct gdtr));
    local_gdt->limit = gdt.limit;
    uint64_t local_gdt_base = (uint64_t)gdt.ptr;
    if (stivale2_hdr.flags & (1 << 1)) {
        local_gdt_base += direct_map_offset;
    }
    local_gdt->ptr = local_gdt_base;
#if defined (__i386__)
    local_gdt->ptr_hi = local_gdt_base >> 32;
#endif

    if (stivale2_hdr.entry_point != 0)
        entry_point = stivale2_hdr.entry_point;

    if (verbose) {
        print("stivale2: Kernel slide: %X\n", slide);

        print("stivale2: Entry point at: %X\n", entry_point);
        print("stivale2: Requested stack at: %X\n", stivale2_hdr.stack);
    }

    // The spec says the stack has to be 16-byte aligned
    if ((stivale2_hdr.stack & (16 - 1)) != 0) {
        print("stivale2: WARNING: Requested stack is not 16-byte aligned\n");
    }

    // It also says the stack cannot be NULL for 32-bit kernels
    if (bits == 32 && stivale2_hdr.stack == 0) {
        panic(true, "stivale2: The stack cannot be 0 for 32-bit kernels");
    }

    strcpy(stivale2_struct->bootloader_brand, "Limine");
    strcpy(stivale2_struct->bootloader_version, LIMINE_VERSION);

    //////////////////////////////////////////////
    // Create boot volume tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_boot_volume *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_boot_volume));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_BOOT_VOLUME_ID;

    if (kernel_volume->guid_valid) {
        tag->flags |= (1 << 0);
        memcpy(&tag->guid, &kernel_volume->guid, sizeof(struct stivale2_guid));
    }

    if (kernel_volume->part_guid_valid) {
        tag->flags |= (1 << 1);
        memcpy(&tag->part_guid, &kernel_volume->part_guid, sizeof(struct stivale2_guid));
    }

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create kernel file struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_kernel_file *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_kernel_file));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_KERNEL_FILE_ID;

    tag->kernel_file = REPORTED_ADDR((uint64_t)(uintptr_t)kernel);

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create kernel file v2 struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_kernel_file_v2 *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_kernel_file_v2));

    tag->tag.identifier = STIVALE2_STRUCT_TAG_KERNEL_FILE_V2_ID;
    tag->kernel_file = REPORTED_ADDR((uint64_t)(uintptr_t)kernel);
    tag->kernel_size = kernel_file_size;

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create kernel slide struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_kernel_slide *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_kernel_slide));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_KERNEL_SLIDE_ID;

    tag->kernel_slide = slide;

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
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

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
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
        struct conf_tuple conf_tuple =
                config_get_tuple(config, i, "MODULE_PATH", "MODULE_STRING");

        char *module_path = conf_tuple.value1;
        char *module_string = conf_tuple.value2;

        struct stivale2_module *m = &tag->modules[i];

        // TODO: perhaps change the module string to to be a pointer.
        //
        // NOTE: By default, the module string is the file name.
        if (module_string == NULL) {
            size_t str_len = strlen(module_path);

            if (str_len > 127)
                str_len = 127;

            memcpy(m->string, module_path, str_len);
        } else {
            size_t str_len = strlen(module_string);

            if (str_len > 127)
                str_len = 127;

            memcpy(m->string, module_string, str_len);
        }

        print("stivale2: Loading module `%s`...\n", module_path);

        struct file_handle *f;
        if ((f = uri_open(module_path)) == NULL)
            panic(true, "stivale2: Failed to open module with path `%s`. Is the path correct?", module_path);

        m->begin = REPORTED_ADDR((uint64_t)(size_t)freadall(f, STIVALE2_MMAP_KERNEL_AND_MODULES));
        m->end   = m->begin + f->size;

        fclose(f);

        if (verbose) {
            print("stivale2: Requested module %u:\n", i);
            print("          Path:   %s\n", module_path);
            print("          String: %s\n", m->string);
            print("          Begin:  %X\n", m->begin);
            print("          End:    %X\n", m->end);
        }
    }

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
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

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
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

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create cmdline struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_cmdline *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_cmdline));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_CMDLINE_ID;

    tag->cmdline = REPORTED_ADDR((uint64_t)(size_t)cmdline);

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create epoch struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_epoch *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_epoch));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_EPOCH_ID;

    tag->epoch = time();
    printv("stivale2: Current epoch: %U\n", tag->epoch);

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // Create framebuffer struct tag
    //////////////////////////////////////////////
    {

    struct fb_info *fb = NULL;
    struct fb_info _fb;

    struct stivale2_header_tag_framebuffer *hdrtag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_FRAMEBUFFER_ID);

    size_t req_width = 0, req_height = 0, req_bpp = 0;

    if (hdrtag != NULL) {
        req_width = hdrtag->framebuffer_width;
        req_height = hdrtag->framebuffer_height;
        req_bpp = hdrtag->framebuffer_bpp;
    }

    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL)
        parse_resolution(&req_width, &req_height, &req_bpp, resolution);

    struct stivale2_header_tag_any_video *avtag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_ANY_VIDEO_ID);

#if uefi == 1
    if (hdrtag == NULL && avtag == NULL) {
        panic(true, "stivale2: Cannot use text mode with UEFI.");
    }
#endif

    char *textmode_str = config_get_value(config, 0, "TEXTMODE");
    bool textmode = textmode_str != NULL && strcmp(textmode_str, "yes") == 0;

    int preference = 0;
    if (avtag != NULL) {
        preference = textmode ? 1 : avtag->preference;
    }

    struct stivale2_header_tag_terminal *terminal_hdr_tag = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_TERMINAL_ID);

    if (bits == 64 && terminal_hdr_tag != NULL) {
        quiet = false;
        serial = false;

        if (bios &&
          ((avtag == NULL && hdrtag == NULL) || (avtag != NULL && preference == 1))) {
            term_textmode();
            textmode = true;
        } else {
#if uefi == 1
            gop_force_16 = true;
#endif
            term_vbe(req_width, req_height);

            if (current_video_mode < 0) {
                panic(true, "stivale2: Failed to initialise terminal");
            }

            fb = &fbinfo;

            textmode = false;
        }

        struct stivale2_struct_tag_terminal *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_terminal));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_TERMINAL_ID;

        if (terminal_hdr_tag->flags & (1 << 0)) {
            // We provide callback
            tag->flags |= (1 << 2);
            if (terminal_hdr_tag->callback != 0) {
#if defined (__i386__)
                term_callback = stivale2_term_callback;
                stivale2_term_callback_ptr = terminal_hdr_tag->callback;
#elif defined (__x86_64__)
                term_callback = (void *)terminal_hdr_tag->callback;
#endif
            }
        }

        // We provide max allowed string length
        tag->flags |= (1 << 1);
        tag->max_length = 0;

        // We provide context control
        tag->flags |= (1 << 3);

#if defined (__i386__)
        if (stivale2_rt_stack == NULL) {
            stivale2_rt_stack = ext_mem_alloc(8192) + 8192;
        }

        stivale2_term_write_ptr = (uintptr_t)term_write;
        tag->term_write = (uintptr_t)(void *)stivale2_term_write_entry;
#elif defined (__x86_64__)
        tag->term_write = (uintptr_t)term_write;
#endif

        // We provide rows and cols
        tag->flags |= (1 << 0);
        tag->cols = term_cols;
        tag->rows = term_rows;

        append_tag(stivale2_struct, (struct stivale2_tag *)tag);

        if (textmode) {
#if bios == 1
            goto have_tm_tag;
#endif
        } else {
            goto have_fb_tag;
        }
    } else {
        fb = &_fb;
    }

    term_deinit();

    if (hdrtag != NULL || (avtag != NULL && uefi) || (avtag != NULL && preference == 0)) {
        term_deinit();

#if uefi == 1
        gop_force_16 = true;
#endif
        if (fb_init(fb, req_width, req_height, req_bpp)) {
have_fb_tag:;
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

            append_tag(stivale2_struct, (struct stivale2_tag *)tag);
        }
    } else {
#if bios == 1
        size_t rows, cols;
        init_vga_textmode(&rows, &cols, false);

have_tm_tag:;
        struct stivale2_struct_tag_textmode *tmtag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_textmode));
        tmtag->tag.identifier = STIVALE2_STRUCT_TAG_TEXTMODE_ID;

        tmtag->address = 0xb8000;
        tmtag->rows = 25;
        tmtag->cols = 80;
        tmtag->bytes_per_char = 2;

        append_tag(stivale2_struct, (struct stivale2_tag *)tmtag);
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

        append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

    //////////////////////////////////////////////
    // Create HHDM struct tag
    //////////////////////////////////////////////
    {
    struct stivale2_struct_tag_hhdm *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_hhdm));
    tag->tag.identifier = STIVALE2_STRUCT_TAG_HHDM_ID;

    tag->addr = direct_map_offset;

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

#if bios == 1
    //////////////////////////////////////////////
    // Create PXE struct tag
    //////////////////////////////////////////////
    if (boot_volume->pxe) {
        struct stivale2_struct_tag_pxe_server_info *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_pxe_server_info));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_PXE_SERVER_INFO;
        tag->server_ip = get_boot_server_info();
        append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }
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

        append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

    //////////////////////////////////////////////
    // Create PMRs struct tag
    //////////////////////////////////////////////
    {
    if (want_fully_virtual) {
        struct stivale2_struct_tag_kernel_base_address *tag =
            ext_mem_alloc(sizeof(struct stivale2_struct_tag_kernel_base_address));

        tag->tag.identifier = STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID;

        tag->physical_base_address = physical_base;
        tag->virtual_base_address = virtual_base;

        append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }
    }

    //////////////////////////////////////////////
    // Create EFI system table struct tag
    //////////////////////////////////////////////
#if uefi == 1
    {
        struct stivale2_struct_tag_efi_system_table *tag = ext_mem_alloc(sizeof(struct stivale2_struct_tag_efi_system_table));
        tag->tag.identifier = STIVALE2_STRUCT_TAG_EFI_SYSTEM_TABLE_ID;

        tag->system_table = REPORTED_ADDR((uint64_t)(uintptr_t)gST);

        append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }
#endif

    bool unmap_null = get_tag(&stivale2_hdr, STIVALE2_HEADER_TAG_UNMAP_NULL_ID) ? true : false;

    pagemap_t pagemap = {0};
    if (bits == 64)
        pagemap = stivale_build_pagemap(want_5lv, unmap_null,
                                        want_pmrs ? ranges : NULL,
                                        want_pmrs ? ranges_count : 0,
                                        want_fully_virtual, physical_base, virtual_base,
                                        direct_map_offset);

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
                            pagemap, smp_hdr_tag->flags & 1, want_pmrs,
                            stivale2_hdr.flags & (1 << 1) ? direct_map_offset : 0,
                            want_pmrs);

        if (smp_info != NULL) {
            tag->tag.identifier = STIVALE2_STRUCT_TAG_SMP_ID;
            tag->bsp_lapic_id   = bsp_lapic_id;
            tag->cpu_count      = cpu_count;
            tag->flags         |= (smp_hdr_tag->flags & 1) && x2apic_check();

            append_tag(stivale2_struct, (struct stivale2_tag *)tag);
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

    // Reserve 32K at 0x70000, if possible
    if (!memmap_alloc_range(0x70000, 0x8000, MEMMAP_USABLE, true, false, false, false)) {
        if ((stivale2_hdr.flags & (1 << 4)) == 0) {
            panic(false, "stivale2: Could not allocate low memory area");
        }
    }

    size_t mmap_entries;
    struct e820_entry_t *mmap = get_memmap(&mmap_entries);

    if (mmap_entries > 256) {
        panic(false, "stivale2: Too many memory map entries!");
    }

    tag->tag.identifier = STIVALE2_STRUCT_TAG_MEMMAP_ID;
    tag->entries = (uint64_t)mmap_entries;

    memcpy((void*)tag + sizeof(struct stivale2_struct_tag_memmap),
           mmap, sizeof(struct e820_entry_t) * mmap_entries);

    append_tag(stivale2_struct, (struct stivale2_tag *)tag);
    }

    //////////////////////////////////////////////
    // List tags
    //////////////////////////////////////////////
    if (verbose) {
        print("stivale2: Generated tags:\n");
        struct stivale2_tag *taglist =
                    (void*)(uintptr_t)(stivale2_struct->tags - ((stivale2_hdr.flags & (1 << 1)) ? direct_map_offset : 0));
        for (size_t i = 0; ; i++) {
            print("          Tag #%u  ID: %X\n", i, taglist->identifier);
            if (taglist->next) {
                taglist = (void*)(uintptr_t)(taglist->next - ((stivale2_hdr.flags & (1 << 1)) ? direct_map_offset : 0));
            } else {
                break;
            }
        }
    }

    // Clear terminal for kernels that will use the stivale2 terminal
    term_write((uint64_t)(uintptr_t)("\e[2J\e[H"), 7);

    term_runtime = true;

    stivale_spinup(bits, want_5lv, &pagemap, entry_point,
                   REPORTED_ADDR((uint64_t)(uintptr_t)stivale2_struct),
                   stivale2_hdr.stack, want_pmrs, want_pmrs, (uintptr_t)local_gdt);
}
