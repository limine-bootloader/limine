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
#include <mm/pmm.h>

static uint8_t* multiboot2_info_buffer = NULL;
static uint32_t multiboot2_info_size = 0;

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

static void* push_boot_param(void* data, uint32_t size) {
    // Align up the allocation size to 8-bytes
    uint32_t alloc_size = ALIGN_UP(size, MULTIBOOT_TAG_ALIGN); 

    // Allocate the multiboot2 info buffer.
    if (multiboot2_info_buffer == NULL) {
        multiboot2_info_buffer = ext_mem_alloc(alloc_size);
    } else {
        uint8_t* old = multiboot2_info_buffer;
        multiboot2_info_buffer = ext_mem_alloc(alloc_size + multiboot2_info_size);
        memcpy(multiboot2_info_buffer, old, multiboot2_info_size);

        // TODO: Free the old allocated buffer. Currently cannot do that since
        // we do not have ext_mem_free and unmap_page yet.
    }

    // Copy the data to the buffer.
    if (data != NULL) {
        memcpy(multiboot2_info_buffer + multiboot2_info_size, data, size);
    }

    // Save the base we allocated from.
    uint8_t* base = multiboot2_info_buffer + multiboot2_info_size;

    // Update the size.
    multiboot2_info_size += alloc_size;

    // Return the base address of the multiboot2 tag we
    // allocated.
    return base;
}

void multiboot2_load(char *config, char* cmdline) {
    struct file_handle *kernel_file = ext_mem_alloc(sizeof(*kernel_file));

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic("multiboot2: KERNEL_PATH not specified");

    print("multiboot2: loading kernel `%s`...\n", kernel_path);

    if (!uri_open(kernel_file, kernel_path))
        panic("multiboot2: failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_USABLE);
    struct multiboot_header* header = load_multiboot2_header(kernel);

    uint32_t entry_point;
    uint32_t kernel_top;

    int bits = elf_bits(kernel);
    struct elf_section_hdr_info section_hdr_info;

    switch (bits) {
        case 32:
            if (elf32_load(kernel, &entry_point, &kernel_top, MEMMAP_KERNEL_AND_MODULES))
                panic("multiboot1: ELF32 load failure");

            elf32_section_hdr_info(kernel, &section_hdr_info);
            break;
        case 64: {
            uint64_t e, t;
            if (elf64_load(kernel, &e, &t, NULL, MEMMAP_KERNEL_AND_MODULES, false, true, NULL, NULL))
                panic("multiboot1: ELF64 load failure");
            
            entry_point = e;
            kernel_top = t;

            elf64_section_hdr_info(kernel, &section_hdr_info);
            break;
        }
        default:
            panic("multiboot1: invalid ELF file bitness");
    }

    print("multiboot2: found kernel entry point at: %X\n", entry_point);
    
    // Iterate through the entries...
    for (struct multiboot_header_tag* tag = (struct multiboot_header_tag*)(header + 1);
         tag < (struct multiboot_header_tag*)((uintptr_t)header + header->header_length) && tag->type != MULTIBOOT_HEADER_TAG_END;
         tag = (struct multiboot_header_tag*)((uintptr_t)tag + ALIGN_UP(tag->size, MULTIBOOT_TAG_ALIGN))) {
  
        switch (tag->type) {
            default: panic("multiboot2: unknown tag type");
        }
    }

    uint32_t start_size = sizeof(struct multiboot2_start_tag*);
    struct multiboot2_start_tag* mbi_start = (struct multiboot2_start_tag*)push_boot_param(NULL, start_size);

    //////////////////////////////////////////////
    // Create command line tag
    //////////////////////////////////////////////
    {
        uint32_t size = strlen(cmdline) + 1 + offsetof(struct multiboot_tag_string, string);
        struct multiboot_tag_string* tag = (struct multiboot_tag_string*)push_boot_param(NULL, size);
    
        tag->type = MULTIBOOT_TAG_TYPE_CMDLINE;
        tag->size = size;

        strcpy(tag->string, cmdline);
    }

    //////////////////////////////////////////////
    // Create bootloader name tag
    //////////////////////////////////////////////
    {
        char* brand = "Limine";
        uint32_t size = sizeof(brand) + offsetof(struct multiboot_tag_string, string);
        struct multiboot_tag_string* tag = (struct multiboot_tag_string*)push_boot_param(NULL, size);

        tag->type = MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME;
        tag->size = size;

        strcpy(tag->string, brand);
    }

    //////////////////////////////////////////////
    // Create framebuffer tag
    //////////////////////////////////////////////
    {
    }

    //////////////////////////////////////////////
    // Create ELF info tag
    //////////////////////////////////////////////
    {
        // ADD ME
    }

#if uefi == 1
    efi_exit_boot_services();
#endif

    //////////////////////////////////////////////
    // Create bootloader memory map tag
    //////////////////////////////////////////////
    {
        size_t mb_mmap_count;
        struct e820_entry_t *raw_memmap = get_raw_memmap(&mb_mmap_count);

        // 1. Create the normal memory map tag.
        uint32_t mmap_size = sizeof(struct multiboot_tag_mmap) + sizeof(struct multiboot_mmap_entry) * mb_mmap_count;
        struct multiboot_tag_mmap* mmap_tag = (struct multiboot_tag_mmap*)push_boot_param(NULL, mmap_size);
        
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
    }

    //////////////////////////////////////////////
    // Create end tag
    //////////////////////////////////////////////
    {
        struct multiboot_tag* end_tag = push_boot_param(NULL, sizeof(struct multiboot_tag));
        end_tag->type = MULTIBOOT_TAG_TYPE_END;
        end_tag->size = sizeof(struct multiboot_tag);
    }

    mbi_start->size = multiboot2_info_size;
    mbi_start->reserved = 0x00;

    common_spinup(multiboot2_spinup_32, 2,
                    entry_point, (uint32_t)(uintptr_t)multiboot2_info_buffer);
}
