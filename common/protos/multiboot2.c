#include <stdint.h>
#include <stddef.h>
#include <protos/multiboot2.h>
#include <protos/multiboot.h>
#include <config.h>
#include <lib/libc.h>
#include <lib/elf.h>
#include <lib/blib.h>
#include <lib/config.h>
#include <lib/print.h>
#include <lib/uri.h>
#include <lib/fb.h>
#include <lib/term.h>
#include <lib/elsewhere.h>
#include <sys/pic.h>
#include <sys/cpu.h>
#include <sys/idt.h>
#include <fs/file.h>
#include <mm/vmm.h>
#include <lib/acpi.h>
#include <mm/pmm.h>
#include <lib/blib.h>
#include <drivers/vga_textmode.h>

#define LIMINE_BRAND "Limine " LIMINE_VERSION

/// Returns the size required to store the multiboot2 info.
static size_t get_multiboot2_info_size(
    char *cmdline,
    size_t modules_size,
    uint32_t section_entry_size, uint32_t section_num,
    uint32_t smbios_tag_size
) {
    return ALIGN_UP(sizeof(struct multiboot2_start_tag), MULTIBOOT_TAG_ALIGN) +                                         // start
        ALIGN_UP(sizeof(struct multiboot_tag_string) + strlen(cmdline) + 1, MULTIBOOT_TAG_ALIGN) +                      // cmdline
        ALIGN_UP(sizeof(struct multiboot_tag_string) + sizeof(LIMINE_BRAND), MULTIBOOT_TAG_ALIGN) +                   // bootloader brand
        ALIGN_UP(sizeof(struct multiboot_tag_framebuffer), MULTIBOOT_TAG_ALIGN) +                                       // framebuffer
        ALIGN_UP(sizeof(struct multiboot_tag_new_acpi) + sizeof(struct rsdp), MULTIBOOT_TAG_ALIGN) +                    // new ACPI info
        ALIGN_UP(sizeof(struct multiboot_tag_old_acpi) + 20, MULTIBOOT_TAG_ALIGN) +                                     // old ACPI info
        ALIGN_UP(sizeof(struct multiboot_tag_elf_sections) + section_entry_size * section_num, MULTIBOOT_TAG_ALIGN) +                   // ELF info
        ALIGN_UP(modules_size, MULTIBOOT_TAG_ALIGN) +                                                                   // modules
        ALIGN_UP(smbios_tag_size, MULTIBOOT_TAG_ALIGN) +                                                                // SMBIOS
        ALIGN_UP(sizeof(struct multiboot_tag_basic_meminfo), MULTIBOOT_TAG_ALIGN) +                                     // basic memory info
        ALIGN_UP(sizeof(struct multiboot_tag_mmap) + sizeof(struct multiboot_mmap_entry) * 256, MULTIBOOT_TAG_ALIGN) +  // MMAP
        #if uefi == 1
            ALIGN_UP(sizeof(struct multiboot_tag_efi_mmap) + (efi_desc_size * 256), MULTIBOOT_TAG_ALIGN) +              // EFI MMAP
            #if defined (__i386__)
                ALIGN_UP(sizeof(struct multiboot_tag_efi32), MULTIBOOT_TAG_ALIGN) +                                     // EFI system table 32
                ALIGN_UP(sizeof(struct multiboot_tag_efi32_ih), MULTIBOOT_TAG_ALIGN) +                                  // EFI image handle 32
            #elif defined (__x86_64__)
                ALIGN_UP(sizeof(struct multiboot_tag_efi64), MULTIBOOT_TAG_ALIGN) +                                     // EFI system table 64
                ALIGN_UP(sizeof(struct multiboot_tag_efi64_ih), MULTIBOOT_TAG_ALIGN) +                                  // EFI image handle 64
            #endif
        #endif
        ALIGN_UP(sizeof(struct multiboot_tag), MULTIBOOT_TAG_ALIGN);                                                    // end
}

#define append_tag(P, TAG) ({ (P) += ALIGN_UP((TAG)->size, MULTIBOOT_TAG_ALIGN); })

bool multiboot2_load(char *config, char* cmdline) {
    struct file_handle *kernel_file;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "multiboot2: KERNEL_PATH not specified");

    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic(true, "multiboot2: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_KERNEL_AND_MODULES);

    size_t kernel_file_size = kernel_file->size;

    fclose(kernel_file);

    struct multiboot_header *header;

    for (size_t header_offset = 0; header_offset < MULTIBOOT_SEARCH; header_offset += MULTIBOOT_HEADER_ALIGN) {
        header = (void *)(kernel + header_offset);

        if (header->magic == MULTIBOOT2_HEADER_MAGIC) {
            break;
        }
    }

    if (header->magic != MULTIBOOT2_HEADER_MAGIC) {
        pmm_free(kernel_file, kernel_file_size);
        return false;
    }

    print("multiboot2: Loading kernel `%s`...\n", kernel_path);

    if (header->magic + header->architecture + header->checksum + header->header_length) {
        panic(true, "multiboot2: Header checksum is invalid");
    }

    struct multiboot_header_tag_address *addresstag = NULL;
    struct multiboot_header_tag_framebuffer *fbtag = NULL;

    bool is_new_acpi_required = false;
    bool is_old_acpi_required = false;

    bool is_elf_info_requested = false;

    uint64_t entry_point = 0xffffffff;

    // Iterate through the entries...
    for (struct multiboot_header_tag *tag = (struct multiboot_header_tag*)(header + 1); // header + 1 to skip the header struct.
       tag < (struct multiboot_header_tag *)((uintptr_t)header + header->header_length) && tag->type != MULTIBOOT_HEADER_TAG_END;
       tag = (struct multiboot_header_tag *)((uintptr_t)tag + ALIGN_UP(tag->size, MULTIBOOT_TAG_ALIGN))) {
        switch (tag->type) {
            case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST: {
                // Iterate the requests and check if they are supported by or not.
                struct multiboot_header_tag_information_request *request = (void *)tag;
                uint32_t size = (request->size - sizeof(struct multiboot_header_tag_information_request)) / sizeof(uint32_t);
                bool is_required = !(request->flags & MULTIBOOT_HEADER_TAG_OPTIONAL);

                for (uint32_t i = 0; i < size; i++) {
                    uint32_t r = request->requests[i];

                    switch (r) {
                        // We already support the following requests:
                        case MULTIBOOT_TAG_TYPE_CMDLINE:
                        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                        case MULTIBOOT_TAG_TYPE_MODULE:
                        case MULTIBOOT_TAG_TYPE_MMAP:
                        case MULTIBOOT_TAG_TYPE_SMBIOS:
                        case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO:
                        #if uefi == 1
                            case MULTIBOOT_TAG_TYPE_EFI_MMAP:
                            #if defined (__i386__)
                                case MULTIBOOT_TAG_TYPE_EFI32:
                                case MULTIBOOT_TAG_TYPE_EFI32_IH:
                            #elif defined (__x86_64__)
                                case MULTIBOOT_TAG_TYPE_EFI64:
                                case MULTIBOOT_TAG_TYPE_EFI64_IH:
                            #endif
                        #endif
                        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                            break;
                        case MULTIBOOT_TAG_TYPE_ACPI_NEW:
                            is_new_acpi_required = is_required;
                            break;
                        case MULTIBOOT_TAG_TYPE_ACPI_OLD:
                            is_old_acpi_required = is_required;
                            break;
                        case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
                            is_elf_info_requested = is_required;
                            break;
                        default:
                            if (is_required)
                                panic(true, "multiboot2: Requested tag `%d` which is not supported", r);
                            break;
                    }
                }
                break;
            }
            case MULTIBOOT_HEADER_TAG_FRAMEBUFFER: {
                fbtag = (void *)tag;
                break;
            }
            case MULTIBOOT_HEADER_TAG_ENTRY_ADDRESS: {
                struct multiboot_header_tag_entry_address *entrytag = (void *)tag;
                entry_point = entrytag->entry_addr;
                break;
            }
            case MULTIBOOT_HEADER_TAG_ADDRESS: {
                addresstag = (void *)tag;
                break;
            }
            // We always align the modules ;^)
            case MULTIBOOT_HEADER_TAG_MODULE_ALIGN:
            case MULTIBOOT_HEADER_TAG_EFI_BS:
                break;

            default: panic(true, "multiboot2: Unknown header tag type");
        }
    }

    struct elsewhere_range *ranges;
    uint64_t ranges_count;

    if (addresstag != NULL) {
        if (addresstag->load_addr > addresstag->header_addr)
            panic(true, "multiboot2: Illegal load address");

        size_t load_size = 0;

        if (addresstag->load_end_addr)
            load_size = addresstag->load_end_addr - addresstag->load_addr;
        else
            load_size = kernel_file_size;

        size_t header_offset = (size_t)header - (size_t)kernel;

        memmap_alloc_range(addresstag->load_addr, load_size, MEMMAP_KERNEL_AND_MODULES, true, true, false, false);
        memcpy((void *)(uintptr_t)addresstag->load_addr, kernel + (header_offset
                - (addresstag->header_addr - addresstag->load_addr)), load_size);

        //kernel_top = addresstag->load_addr + load_size;

        if (addresstag->bss_end_addr) {
            uintptr_t bss_addr = addresstag->load_addr + load_size;
            if (addresstag->bss_end_addr < bss_addr)
                panic(true, "multiboot2: Illegal bss end address");

            uint32_t bss_size = addresstag->bss_end_addr - bss_addr;

            memmap_alloc_range(bss_addr, bss_size, MEMMAP_KERNEL_AND_MODULES, true, true, false, false);
            memset((void *)bss_addr, 0, bss_size);

            //kernel_top = bss_addr + bss_size;
        }
    } else {
        uint64_t e;
        int bits = elf_bits(kernel);

        switch (bits) {
            case 32:
                if (elf32_load_elsewhere(kernel, &e, &ranges, &ranges_count))
                    panic(true, "multiboot2: ELF32 load failure");

                break;
            case 64: {
                if (elf64_load_elsewhere(kernel, &e, &ranges, &ranges_count))
                    panic(true, "multiboot2: ELF64 load failure");

                break;
            }
            default:
                panic(true, "multiboot2: Invalid ELF file bitness");
        }

        if (entry_point == 0xffffffff) {
            entry_point = e;
        }
    }

    struct elf_section_hdr_info *section_hdr_info = NULL;
    int bits = elf_bits(kernel);

    switch (bits) {
        case 32:
            section_hdr_info = elf32_section_hdr_info(kernel);
            break;
        case 64: {
            section_hdr_info = elf64_section_hdr_info(kernel);
            break;
        }
    }

    size_t modules_size = 0;
    size_t n_modules;

    for (n_modules = 0;; n_modules++) {
        struct conf_tuple conf_tuple = config_get_tuple(config, n_modules, "MODULE_PATH", "MODULE_STRING");
        if (!conf_tuple.value1) break;

        char *module_cmdline = conf_tuple.value2;
        if (!module_cmdline) module_cmdline = "";
        modules_size += sizeof(struct multiboot_tag_module) + strlen(module_cmdline) + 1;
    }

    struct smbios_entry_point_32* smbios_entry_32 = NULL;
    struct smbios_entry_point_64* smbios_entry_64 = NULL;

    acpi_get_smbios((void **)&smbios_entry_32, (void **)&smbios_entry_64);

    uint32_t smbios_tag_size = 0;

    if (smbios_entry_32 != NULL)
        smbios_tag_size += sizeof(struct multiboot_tag_smbios) + smbios_entry_32->length;
    if (smbios_entry_64 != NULL)
        smbios_tag_size += sizeof(struct multiboot_tag_smbios) + smbios_entry_64->length;

    size_t mb2_info_size = get_multiboot2_info_size(
        cmdline,
        modules_size,
        section_hdr_info ? section_hdr_info->section_entry_size : 0,
        section_hdr_info ? section_hdr_info->num : 0,
        smbios_tag_size
    );

    size_t info_idx = 0;

    // Realloc elsewhere ranges to include mb2 info, modules, and elf sections
    struct elsewhere_range *new_ranges = ext_mem_alloc(sizeof(struct elsewhere_range) *
        (ranges_count
       + 1 /* mb2 info range */
       + n_modules
       + (section_hdr_info ? section_hdr_info->num : 0)));

    memcpy(new_ranges, ranges, sizeof(struct elsewhere_range) * ranges_count);
    pmm_free(ranges, sizeof(struct elsewhere_range) * ranges_count);
    ranges = new_ranges;

    // GRUB allocates boot info at 0x10000, *except* if the kernel happens
    // to overlap this region, then it gets moved to right after the
    // kernel, or whichever PHDR happens to sit at 0x10000.
    // Allocate it wherever, then move it to where GRUB puts it
    // afterwards.

    // Elsewhere append mb2 info *after* kernel but *before* modules.
    uint8_t *mb2_info = ext_mem_alloc(mb2_info_size);
    uint64_t mb2_info_final_loc = 0x10000;

    elsewhere_append(true /* flexible target */,
            ranges, &ranges_count,
            mb2_info, &mb2_info_final_loc, mb2_info_size);

    struct multiboot2_start_tag *mbi_start = (struct multiboot2_start_tag *)mb2_info;
    info_idx += sizeof(struct multiboot2_start_tag);

    //////////////////////////////////////////////
    // Create ELF info tag
    //////////////////////////////////////////////
    if (section_hdr_info == NULL) {
        if (is_elf_info_requested) {
            panic(true, "multiboot2: Cannot return ELF file information");
        }
    } else {
        uint32_t size = sizeof(struct multiboot_tag_elf_sections) + section_hdr_info->section_entry_size * section_hdr_info->num;
        struct multiboot_tag_elf_sections *tag = (struct multiboot_tag_elf_sections*)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_ELF_SECTIONS;
        tag->size = size;

        tag->num = section_hdr_info->num;
        tag->entsize = section_hdr_info->section_entry_size;
        tag->shndx = section_hdr_info->str_section_idx;

        memcpy(tag->sections, kernel + section_hdr_info->section_offset, section_hdr_info->section_entry_size * section_hdr_info->num);

        for (size_t i = 0; i < section_hdr_info->num; i++) {
            struct elf64_shdr *shdr = (void *)tag->sections + i * section_hdr_info->section_entry_size;

            if (shdr->sh_addr != 0 || shdr->sh_size == 0) {
                continue;
            }

            uint64_t section = (uint64_t)-1; /* no target preference, use top */

            elsewhere_append(true /* flexible target */,
                    ranges, &ranges_count,
                    kernel + shdr->sh_offset, &section, shdr->sh_size);

            shdr->sh_addr = section;
        }

        append_tag(info_idx, tag);
    }

    //////////////////////////////////////////////
    // Create modules tag
    //////////////////////////////////////////////
    for (size_t i = 0; i < n_modules; i++) {
        struct conf_tuple conf_tuple = config_get_tuple(config, i, "MODULE_PATH", "MODULE_STRING");
        char *module_path = conf_tuple.value1;
        if (!module_path) panic(true, "multiboot2: Module disappeared unexpectedly");

        print("multiboot2: Loading module `%s`...\n", module_path);

        struct file_handle *f;
        if ((f = uri_open(module_path)) == NULL)
            panic(true, "multiboot2: Failed to open module with path `%s`. Is the path correct?", module_path);


        // Module commandline can be null, so we guard against that and make the
        // string "".
        char *module_cmdline = conf_tuple.value2;
        if (!module_cmdline) module_cmdline = "";

        void *module_addr = freadall(f, MEMMAP_BOOTLOADER_RECLAIMABLE);
        uint64_t module_target = (uint64_t)-1;

        elsewhere_append(true /* flexible target */,
                ranges, &ranges_count,
                module_addr, &module_target, f->size);

        struct multiboot_tag_module *module_tag = (struct multiboot_tag_module *)(mb2_info + info_idx);

        module_tag->type = MULTIBOOT_TAG_TYPE_MODULE;
        module_tag->size = sizeof(struct multiboot_tag_module) + strlen(module_cmdline) + 1;
        module_tag->mod_start   = module_target;
        module_tag->mod_end     = module_tag->mod_start + f->size;
        strcpy(module_tag->cmdline, module_cmdline); // Copy over the command line

        fclose(f);

        if (verbose) {
            print("multiboot2: Requested module %u:\n", i);
            print("            Path:   %s\n", module_path);
            print("            String: \"%s\"\n", module_cmdline ?: "");
            print("            Begin:  %x\n", module_tag->mod_start);
            print("            End:    %x\n", module_tag->mod_end);
        }

        append_tag(info_idx, module_tag);
    }

    //////////////////////////////////////////////
    // Create command line tag
    //////////////////////////////////////////////
    {
        uint32_t size = sizeof(struct multiboot_tag_string) + strlen(cmdline) + 1;
        struct multiboot_tag_string *tag = (struct multiboot_tag_string *)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_CMDLINE;
        tag->size = size;

        strcpy(tag->string, cmdline);
        append_tag(info_idx, tag);
    }

    //////////////////////////////////////////////
    // Create bootloader name tag
    //////////////////////////////////////////////
    {
        uint32_t size = sizeof(struct multiboot_tag_string) + sizeof(LIMINE_BRAND);
        struct multiboot_tag_string *tag = (struct multiboot_tag_string *)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
        tag->size = size;

        strcpy(tag->string, LIMINE_BRAND);
        append_tag(info_idx, tag);
    }

    //////////////////////////////////////////////
    // Create EFI image handle tag
    //////////////////////////////////////////////
#if uefi == 1
    {
    #if defined (__i386__)
        struct multiboot_tag_efi64_ih *tag = (struct multiboot_tag_efi64_ih *)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_EFI64_IH;
        tag->size = sizeof(struct multiboot_tag_efi64_ih);
    #elif defined (__x86_64__)
        struct multiboot_tag_efi32_ih *tag = (struct multiboot_tag_efi32_ih *)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_EFI32_IH;
        tag->size = sizeof(struct multiboot_tag_efi32_ih);
    #endif

        tag->pointer = (uintptr_t)efi_image_handle;
        append_tag(info_idx, tag);
    }
#endif

    //////////////////////////////////////////////
    // Create framebuffer tag
    //////////////////////////////////////////////
    {
        term_deinit();

        if (fbtag) {
            size_t req_width = fbtag->width;
            size_t req_height = fbtag->height;
            size_t req_bpp = fbtag->depth;

            char *resolution = config_get_value(config, 0, "RESOLUTION");
            if (resolution != NULL)
                parse_resolution(&req_width, &req_height, &req_bpp, resolution);

            struct multiboot_tag_framebuffer *tag = (struct multiboot_tag_framebuffer *)(mb2_info + info_idx);

            struct fb_info fbinfo;
            if (!fb_init(&fbinfo, req_width, req_height, req_bpp)) {
#if bios == 1
                size_t rows, cols;
                init_vga_textmode(&rows, &cols, false);

                tag->common.framebuffer_addr = 0xb8000;
                tag->common.framebuffer_pitch = 2 * cols;
                tag->common.framebuffer_width = cols;
                tag->common.framebuffer_height = rows;
                tag->common.framebuffer_bpp = 16;
                tag->common.framebuffer_type = MULTIBOOT_FRAMEBUFFER_TYPE_EGA_TEXT;
#elif uefi == 1
                panic(true, "multiboot2: Cannot use text mode with UEFI");
#endif
            } else {
                tag->common.type = MULTIBOOT_TAG_TYPE_FRAMEBUFFER;
                tag->common.size = sizeof(struct multiboot_tag_framebuffer);
                tag->common.framebuffer_addr = fbinfo.framebuffer_addr;
                tag->common.framebuffer_pitch = fbinfo.framebuffer_pitch;
                tag->common.framebuffer_width = fbinfo.framebuffer_width;
                tag->common.framebuffer_height = fbinfo.framebuffer_height;
                tag->common.framebuffer_bpp = fbinfo.framebuffer_bpp;
                tag->common.framebuffer_type = MULTIBOOT_FRAMEBUFFER_TYPE_RGB; // We only support RGB for VBE

                tag->framebuffer_red_field_position = fbinfo.red_mask_shift;
                tag->framebuffer_red_mask_size = fbinfo.red_mask_size;
                tag->framebuffer_green_field_position = fbinfo.green_mask_shift;
                tag->framebuffer_green_mask_size = fbinfo.green_mask_size;
                tag->framebuffer_blue_field_position = fbinfo.blue_mask_shift;
                tag->framebuffer_blue_mask_size = fbinfo.blue_mask_size;
            }

            append_tag(info_idx, &tag->common);
        } else {
#if uefi == 1
            panic(true, "multiboot2: Cannot use text mode with UEFI");
#elif bios == 1
            size_t rows, cols;
            init_vga_textmode(&rows, &cols, false);
#endif
        }
    }

    //////////////////////////////////////////////
    // Create new ACPI info tag
    //////////////////////////////////////////////
    {
        void *new_rsdp = acpi_get_rsdp_v2();

        if (new_rsdp != NULL) {
            uint32_t size = sizeof(struct multiboot_tag_new_acpi) + sizeof(struct rsdp); // XSDP is 36 bytes wide
            struct multiboot_tag_new_acpi *tag = (struct multiboot_tag_new_acpi *)(mb2_info + info_idx);

            tag->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
            tag->size = size;

            memcpy(tag->rsdp, new_rsdp, sizeof(struct rsdp));
            append_tag(info_idx, tag);
        } else if (is_new_acpi_required) {
            panic(true, "multiboot2: XSDP requested but not found");
        }
    }

    //////////////////////////////////////////////
    // Create old ACPI info tag
    //////////////////////////////////////////////
    {
        void *old_rsdp = acpi_get_rsdp_v1();

        if (old_rsdp != NULL) {
            uint32_t size = sizeof(struct multiboot_tag_old_acpi) + 20; // RSDP is 20 bytes wide
            struct multiboot_tag_old_acpi *tag = (struct multiboot_tag_old_acpi *)(mb2_info + info_idx);

            tag->type = MULTIBOOT_TAG_TYPE_ACPI_OLD;
            tag->size = size;

            memcpy(tag->rsdp, old_rsdp, 20);
            append_tag(info_idx, tag);
        } else if (is_old_acpi_required) {
            panic(true, "multiboot2: RSDP requested but not found");
        }
    }

    //////////////////////////////////////////////
    // Create SMBIOS tag
    //////////////////////////////////////////////
    {
        // NOTE: The multiboot2 specification does not say anything about if both
        // smbios 32 and 64 bit entry points are present, then we pass both of them + smbios
        // support for grub2 is unimplemented. So, we are going to assume they expect us to
        // pass both of them if avaliable. Oh well...
        if (smbios_entry_32 != NULL) {
            struct multiboot_tag_smbios *tag = (struct multiboot_tag_smbios *)(mb2_info + info_idx);

            tag->type = MULTIBOOT_TAG_TYPE_SMBIOS;
            tag->size = sizeof(struct multiboot_tag_smbios);

            tag->major = smbios_entry_32->major_version;
            tag->minor = smbios_entry_32->minor_version;

            memset(tag->reserved, 0, 6);
            memcpy(tag->tables, smbios_entry_32, smbios_entry_32->length);

            append_tag(info_idx, tag);
        }

        if (smbios_entry_64 != NULL) {
            struct multiboot_tag_smbios *tag = (struct multiboot_tag_smbios *)(mb2_info + info_idx);

            tag->type = MULTIBOOT_TAG_TYPE_SMBIOS;
            tag->size = sizeof(struct multiboot_tag_smbios);

            tag->major = smbios_entry_64->major_version;
            tag->minor = smbios_entry_64->minor_version;

            memset(tag->reserved, 0, 6);
            memcpy(tag->tables, smbios_entry_64, smbios_entry_64->length);

            append_tag(info_idx, tag);
        }
    }

    //////////////////////////////////////////////
    // Create EFI system table info tag
    //////////////////////////////////////////////
#if uefi == 1
    {
    #if defined (__i386__)
        uint32_t size = sizeof(struct multiboot_tag_efi32);
        struct multiboot_tag_efi32 *tag = (void *)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_EFI32;
    #elif defined (__x86_64__)
        uint32_t size = sizeof(struct multiboot_tag_efi64);
        struct multiboot_tag_efi64 *tag = (void *)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_EFI64;
    #endif

        tag->size = size;
        tag->pointer = (uintptr_t)gST;

        append_tag(info_idx, tag);
    }
#endif

    // Load relocation stub where it won't get overwritten (hopefully)
    size_t reloc_stub_size = (size_t)multiboot_reloc_stub_end - (size_t)multiboot_reloc_stub;
    void *reloc_stub = ext_mem_alloc(reloc_stub_size);
    memcpy(reloc_stub, multiboot_reloc_stub, reloc_stub_size);

#if uefi == 1
    efi_exit_boot_services();
#endif

    size_t mb_mmap_count;
    struct e820_entry_t *raw_memmap = get_raw_memmap(&mb_mmap_count);

    //////////////////////////////////////////////
    // Create memory map tag
    //////////////////////////////////////////////
    {
        if (mb_mmap_count > 256) {
            panic(false, "multiboot2: too many memory map entries");
        }

        // Create the normal memory map tag.
        uint32_t mmap_size = sizeof(struct multiboot_tag_mmap) + sizeof(struct multiboot_mmap_entry) * mb_mmap_count;
        struct multiboot_tag_mmap *mmap_tag = (struct multiboot_tag_mmap *)(mb2_info + info_idx);

        mmap_tag->type = MULTIBOOT_TAG_TYPE_MMAP;
        mmap_tag->entry_size = sizeof(struct multiboot_mmap_entry);
        mmap_tag->entry_version = 0;
        mmap_tag->size = mmap_size;

        for (size_t i = 0; i < mb_mmap_count; i++) {
            struct multiboot_mmap_entry *entry = &mmap_tag->entries[i];
            entry->addr = raw_memmap[i].base;
            entry->len  = raw_memmap[i].length;
            entry->type = raw_memmap[i].type;
            entry->zero = 0;
        }

        append_tag(info_idx, mmap_tag);
    }

    //////////////////////////////////////////////
    // Create basic memory info tag
    //////////////////////////////////////////////
    {
        struct meminfo meminfo = mmap_get_info(mb_mmap_count, raw_memmap);
        struct multiboot_tag_basic_meminfo *tag = (struct multiboot_tag_basic_meminfo *)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_BASIC_MEMINFO;
        tag->size = sizeof(struct multiboot_tag_basic_meminfo);

        // Convert the uppermem and lowermem fields from bytes to
        // KiB.
        tag->mem_upper = (uint32_t)(meminfo.uppermem / 1024);
        tag->mem_lower = (uint32_t)(meminfo.lowermem / 1024);

        append_tag(info_idx, tag);
    }

    //////////////////////////////////////////////
    // Create EFI memory map tag
    //////////////////////////////////////////////
#if uefi == 1
    {
        if ((efi_mmap_size / efi_desc_size) > 256) {
            panic(false, "multiboot2: too many EFI memory map entries");
        }

        // Create the EFI memory map tag.
        uint32_t size = sizeof(struct multiboot_tag_efi_mmap) * efi_mmap_size;
        struct multiboot_tag_efi_mmap *mmap_tag = (struct multiboot_tag_efi_mmap *)(mb2_info + info_idx);

        mmap_tag->type = MULTIBOOT_TAG_TYPE_EFI_MMAP;
        mmap_tag->descr_vers = efi_desc_ver;
        mmap_tag->descr_size = efi_desc_size;
        mmap_tag->size = size;

        // Copy over the EFI memory map.
        memcpy(mmap_tag->efi_mmap, efi_mmap, efi_mmap_size);
        append_tag(info_idx, mmap_tag);
    }
#endif

    //////////////////////////////////////////////
    // Create end tag
    //////////////////////////////////////////////
    {
        struct multiboot_tag *end_tag = (struct multiboot_tag *)(mb2_info + info_idx);
        end_tag->type = MULTIBOOT_TAG_TYPE_END;
        end_tag->size = sizeof(struct multiboot_tag);

        append_tag(info_idx, end_tag);
    }

    mbi_start->size = info_idx;
    mbi_start->reserved = 0x00;

    irq_flush_type = IRQ_PIC_ONLY_FLUSH;

    common_spinup(multiboot_spinup_32, 6,
                  (uint32_t)(uintptr_t)reloc_stub, (uint32_t)0x36d76289,
                  (uint32_t)mb2_info_final_loc, (uint32_t)entry_point,
                  (uint32_t)(uintptr_t)ranges, (uint32_t)ranges_count);
}
