#include <protos/multiboot2.h>
#include <stdint.h>
#include <stddef.h>
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
#include <lib/acpi.h>
#include <mm/pmm.h>
#include <lib/blib.h>
#include <drivers/vga_textmode.h>

static struct multiboot_header* load_multiboot2_header(uint8_t* kernel) {
    struct multiboot_header* ptr = {0};
    struct multiboot_header header;

    size_t header_offset = 0;

    for (header_offset = 0; header_offset < MULTIBOOT_SEARCH; header_offset += MULTIBOOT_HEADER_ALIGN) {
        uint32_t v;
        memcpy(&v, kernel + header_offset, 4);

        if (v == MULTIBOOT2_HEADER_MAGIC) {
            memcpy(&header, kernel + header_offset, sizeof(header));

            ptr = ext_mem_alloc(header.header_length);
            memcpy(ptr, kernel + header_offset, header.header_length);

            break;
        }
    }

    if (ptr->magic != MULTIBOOT2_HEADER_MAGIC) {
        panic("multiboot2: could not find header");
    } else if (ptr->magic + ptr->architecture + ptr->checksum + ptr->header_length) {
        panic("mutliboot2: header checksum is invalid");
    }

    return ptr;
}

/// Returns the size required to store the multiboot2 info.
static size_t get_multiboot2_info_size(
    char* cmdline, 
    size_t modules_size,
    struct elf_section_hdr_info* section_hdr_info
) {
    return ALIGN_UP(sizeof(struct multiboot2_start_tag), MULTIBOOT_TAG_ALIGN) +                                         // start
        ALIGN_UP(strlen(cmdline) + 1 + offsetof(struct multiboot_tag_string, string), MULTIBOOT_TAG_ALIGN) +            // cmdline
        ALIGN_UP(8 + offsetof(struct multiboot_tag_string, string), MULTIBOOT_TAG_ALIGN) +                              // bootloader brand
        ALIGN_UP(sizeof(struct multiboot_tag_framebuffer), MULTIBOOT_TAG_ALIGN) +                                       // framebuffer
        ALIGN_UP(sizeof(struct multiboot_tag_new_acpi) + 36, MULTIBOOT_TAG_ALIGN) +                                     // new ACPI info
        ALIGN_UP(sizeof(struct multiboot_tag_elf_sections) + section_hdr_info->section_hdr_size, MULTIBOOT_TAG_ALIGN) + // ELF info
        ALIGN_UP(modules_size, MULTIBOOT_TAG_ALIGN) +                                                                   // modules
        ALIGN_UP(sizeof(struct multiboot_tag_mmap) + sizeof(struct multiboot_mmap_entry) * 256, MULTIBOOT_TAG_ALIGN) +  // MMAP
#if uefi == 1
        ALIGN_UP(sizeof(struct multiboot_tag_efi_mmap) + (efi_desc_size * 256), MULTIBOOT_TAG_ALIGN) +                  // EFI MMAP
#endif
        ALIGN_UP(sizeof(struct multiboot_tag), MULTIBOOT_TAG_ALIGN);                                                    // end
}

#define append_tag(P, TAG) ({ (P) += ALIGN_UP((TAG)->size, MULTIBOOT_TAG_ALIGN); })

void multiboot2_load(char *config, char* cmdline) {
    struct file_handle *kernel_file = ext_mem_alloc(sizeof(struct file_handle));

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("multiboot2: KERNEL_PATH not specified");

    print("multiboot2: loading kernel `%s`...\n", kernel_path);

    if (!uri_open(kernel_file, kernel_path))
        panic("multiboot2: failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_BOOTLOADER_RECLAIMABLE);
    struct multiboot_header* header = load_multiboot2_header(kernel);

    uint32_t entry_point;
    uint32_t kernel_top;

    int bits = elf_bits(kernel);
    struct elf_section_hdr_info* section_hdr_info;

    switch (bits) {
        case 32:
            section_hdr_info = elf32_section_hdr_info(kernel);

            if (elf32_load(kernel, &entry_point, &kernel_top, MEMMAP_KERNEL_AND_MODULES))
                panic("multiboot2: ELF32 load failure");

            break;
        case 64: {
            section_hdr_info = elf64_section_hdr_info(kernel);

            uint64_t e, t;
            if (elf64_load(kernel, &e, &t, NULL, MEMMAP_KERNEL_AND_MODULES, false, true, NULL, NULL))
                panic("multiboot2: ELF64 load failure");
            
            entry_point = e;
            kernel_top = t;

            break;
        }
        default:
            panic("multiboot2: invalid ELF file bitness");
    }

    print("multiboot2: found kernel entry point at: %x\n", entry_point);
    
    struct multiboot_header_tag_framebuffer *fbtag = NULL;

    bool is_new_acpi_required = false;

    // Iterate through the entries...
    for (struct multiboot_header_tag* tag = (struct multiboot_header_tag*)(header + 1); // header + 1 to skip the header struct.
         tag < (struct multiboot_header_tag*)((uintptr_t)header + header->header_length) && tag->type != MULTIBOOT_HEADER_TAG_END;
         tag = (struct multiboot_header_tag*)((uintptr_t)tag + ALIGN_UP(tag->size, MULTIBOOT_TAG_ALIGN))) {
  
        switch (tag->type) {
            case MULTIBOOT_HEADER_TAG_INFORMATION_REQUEST: {
                // Iterate the requests and check if they are supported by or not.
                struct multiboot_header_tag_information_request* request = (void*)tag;
                uint32_t size = (request->size - sizeof(struct multiboot_header_tag_information_request)) 
                    / sizeof(uint32_t);
                    
                for (uint32_t i = 0; i < size; i++) {
                    uint32_t r = request->requests[i];
                    bool is_required = !(tag->flags & MULTIBOOT_HEADER_TAG_OPTIONAL);

                    switch(r) {
                        // We already support the following requests:
                        case MULTIBOOT_TAG_TYPE_CMDLINE:
                        case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME:
                        case MULTIBOOT_TAG_TYPE_MODULE:
                        case MULTIBOOT_TAG_TYPE_MMAP:
                        case MULTIBOOT_TAG_TYPE_EFI_MMAP:
                        case MULTIBOOT_TAG_TYPE_FRAMEBUFFER:
                        case MULTIBOOT_TAG_TYPE_ELF_SECTIONS:
                            break;

                        case MULTIBOOT_TAG_TYPE_ACPI_NEW: is_new_acpi_required = is_required; break;

                        default: {
                            if (!(request->flags & MULTIBOOT_HEADER_TAG_OPTIONAL))
                                panic("multiboot2: requested tag `%d` which is not supported", r);
                        } break;
                    }
                }
            } break;

            case MULTIBOOT_HEADER_TAG_FRAMEBUFFER: {
                fbtag = (struct multiboot_header_tag_framebuffer*)tag;
            } break;

            // We always align the modules ;^)
            case MULTIBOOT_HEADER_TAG_MODULE_ALIGN: break;

            default: panic("multiboot2: unknown tag type");
        }
    }

    size_t modules_size;
    size_t n_modules = 0;

    for (n_modules = 0;; n_modules++) {
        if (config_get_value(config, modules_size, "MODULE_PATH") == NULL)
            break;
        
        char* module_cmdline = config_get_value(config, modules_size, "MODULE_STRING");
        modules_size += sizeof(struct multiboot_tag_module) + strlen(module_cmdline) + 1;
    }

    size_t mb2_info_size = get_multiboot2_info_size(cmdline, modules_size, section_hdr_info);
    size_t info_idx = 0;
    uint8_t* mb2_info = ext_mem_alloc(mb2_info_size);

    struct multiboot2_start_tag* mbi_start = (struct multiboot2_start_tag*)mb2_info;
    info_idx += sizeof(struct multiboot2_start_tag);

    //////////////////////////////////////////////
    // Create modules tag
    //////////////////////////////////////////////
    for (size_t i = 0; i < n_modules; i++) {
        char* module_path = config_get_value(config, i, "MODULE_PATH");
        if (module_path == NULL)
            panic("multiboot2: Module disappeared unexpectedly");

        print("multiboot2: Loading module `%s`...\n", module_path);

        struct file_handle f;
        if (!uri_open(&f, module_path))
            panic("multiboot2: Failed to open module with path `%s`. Is the path correct?", module_path);
        
        char* module_cmdline = config_get_value(config, i, "MODULE_STRING");
        void* module_addr = (void *)(uintptr_t)ALIGN_UP(kernel_top, 4096);

        // Module commandline can be null, so we guard against that and make the 
        // string "".
        if (module_cmdline == NULL) {
            module_cmdline = "";
        }

        memmap_alloc_range((uintptr_t)module_addr, f.size, MEMMAP_KERNEL_AND_MODULES,
                            true, true, false, false);

        kernel_top = (uintptr_t)module_addr + f.size;
        fread(&f, module_addr, 0, f.size);
    
        struct multiboot_tag_module* module_tag = (struct multiboot_tag_module*)(mb2_info + info_idx);

        module_tag->type = MULTIBOOT_TAG_TYPE_MODULE;
        module_tag->size = sizeof(struct multiboot_tag_module) + strlen(module_cmdline) + 1;
        module_tag->mod_start   = (uint32_t)(size_t)module_addr;
        module_tag->mod_end     = module_tag->mod_start + f.size;
        strcpy(module_tag->cmdline, module_cmdline); // Copy over the command line

        if (verbose) {
            print("multiboot2: Requested module %u:\n", i);
            print("            Path:   %s\n", module_path);
            print("            String: \"%s\"\n", module_cmdline ?: "");
            print("            Begin:  %x\n", module_tag->mod_start);
            print("            End:    %x\n", module_tag->mod_end);
        }

        append_tag(mb2_info, module_tag);
    }

    //////////////////////////////////////////////
    // Create command line tag
    //////////////////////////////////////////////
    {
        uint32_t size = strlen(cmdline) + 1 + offsetof(struct multiboot_tag_string, string);
        struct multiboot_tag_string* tag = (struct multiboot_tag_string*)(mb2_info + info_idx);
    
        tag->type = MULTIBOOT_TAG_TYPE_CMDLINE;
        tag->size = size;

        strcpy(tag->string, cmdline);
        append_tag(info_idx, tag);
    }

    //////////////////////////////////////////////
    // Create bootloader name tag
    //////////////////////////////////////////////
    {
        char* brand = "Limine";
        uint32_t size = sizeof(brand) + offsetof(struct multiboot_tag_string, string);
        struct multiboot_tag_string* tag = (struct multiboot_tag_string*)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
        tag->size = size;

        strcpy(tag->string, brand);
        append_tag(info_idx, tag);
    }

    //////////////////////////////////////////////
    // Create framebuffer tag
    //////////////////////////////////////////////
    {
        if (fbtag) {
            size_t req_width = fbtag->width;
            size_t req_height = fbtag->height;
            size_t req_bpp = 0x00;
            
            char *resolution = config_get_value(config, 0, "RESOLUTION");
            if (resolution != NULL)
                parse_resolution(&req_width, &req_height, &req_bpp, resolution);
            
            struct fb_info fbinfo;
            if (!fb_init(&fbinfo, req_width, req_height, req_bpp))
                panic("mutltiboot2: Unable to set video mode");

            memmap_alloc_range(fbinfo.framebuffer_addr,
                            (uint64_t)fbinfo.framebuffer_pitch * fbinfo.framebuffer_height,
                            MEMMAP_FRAMEBUFFER, false, false, false, true);


            struct multiboot_tag_framebuffer* tag = (struct multiboot_tag_framebuffer*)(mb2_info + info_idx);

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

            info_idx += tag->common.size;
        } else {
#if uefi == 1
            panic("multiboot2: cannot use text mode with UEFI");
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
        void* new_rsdp = acpi_get_rsdp();

        if (new_rsdp != NULL) {
            uint32_t size = sizeof(struct multiboot_tag_new_acpi) + 36; // XSDP is 36 bytes wide
            struct multiboot_tag_new_acpi* tag = (struct multiboot_tag_new_acpi*)(mb2_info + info_idx);

            tag->type = MULTIBOOT_TAG_TYPE_ACPI_NEW;
            tag->size = size;

            memcpy(tag->rsdp, new_rsdp, 36);
            append_tag(info_idx, tag);
        } else if (is_new_acpi_required) {
            panic("multiboot2: new ACPI table not present");
        }
    }

    //////////////////////////////////////////////
    // Create ELF info tag
    //////////////////////////////////////////////
    {
        uint32_t size = sizeof(struct multiboot_tag_elf_sections) + section_hdr_info->section_hdr_size;
        struct multiboot_tag_elf_sections* tag = (struct multiboot_tag_elf_sections*)(mb2_info + info_idx);

        tag->type = MULTIBOOT_TAG_TYPE_ELF_SECTIONS;
        tag->size = size;

        tag->num = section_hdr_info->num;
        tag->entsize = section_hdr_info->section_entry_size;
        tag->shndx = section_hdr_info->str_section_idx;

        memcpy(tag->sections, section_hdr_info->section_hdrs, section_hdr_info->section_hdr_size);
        append_tag(info_idx, tag);
    }

#if uefi == 1
    efi_exit_boot_services();
#endif

    //////////////////////////////////////////////
    // Create memory map tag
    //////////////////////////////////////////////
    {
        size_t mb_mmap_count;
        struct e820_entry_t *raw_memmap = get_raw_memmap(&mb_mmap_count);

        if (mb_mmap_count > 256) {
            panic("multiboot2: too many memory map entries");
        }

        // Create the normal memory map tag.
        uint32_t mmap_size = sizeof(struct multiboot_tag_mmap) + sizeof(struct multiboot_mmap_entry) * mb_mmap_count;
        struct multiboot_tag_mmap* mmap_tag = (struct multiboot_tag_mmap*)(mb2_info + info_idx);
        
        mmap_tag->type = MULTIBOOT_TAG_TYPE_MMAP;
        mmap_tag->entry_size = sizeof(struct multiboot_mmap_entry);
        mmap_tag->entry_version = 0;
        mmap_tag->size = mmap_size;

        for (size_t i = 0; i < mb_mmap_count; i++) {
            struct multiboot_mmap_entry* entry = &mmap_tag->entries[i];
            entry->addr = raw_memmap[i].base;
            entry->len  = raw_memmap[i].length;
            entry->type = raw_memmap[i].type;
            entry->zero = 0;
        }

        append_tag(info_idx, mmap_tag);
    }

    //////////////////////////////////////////////
    // Create EFI memory map tag
    //////////////////////////////////////////////
#if uefi == 1
    {
        if ((efi_mmap_size / efi_desc_size) > 256) {
            panic("multiboot2: too many EFI memory map entries");
        }

        // Create the EFI memory map tag.
        uint32_t size = sizeof(struct multiboot_tag_efi_mmap) * efi_mmap_size;
        struct multiboot_tag_efi_mmap* mmap_tag = (struct multiboot_tag_efi_mmap*)(mb2_info + info_idx);

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
        struct multiboot_tag* end_tag = (struct multiboot_tag*)(mb2_info + info_idx);
        end_tag->type = MULTIBOOT_TAG_TYPE_END;
        end_tag->size = sizeof(struct multiboot_tag);

        append_tag(info_idx, end_tag);
    }

    mbi_start->size = mb2_info_size;
    mbi_start->reserved = 0x00;

    common_spinup(multiboot2_spinup_32, 2,
                    entry_point, (uint32_t)(uintptr_t)mbi_start);
}
