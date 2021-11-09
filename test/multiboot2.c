#include <e9print.h>
#include <stdint.h>
#include <multiboot2.h>

struct multiboot_info {
    uint32_t size;
    uint32_t reserved;
    struct multiboot_tag *first;
};

void multiboot2_main(uint32_t magic, struct multiboot_info* mb_info_addr) {
    if (magic != MULTIBOOT2_BOOTLOADER_MAGIC) {
        e9_printf("multiboot2: Invalid magic: %x\n", magic);
        goto out;
    }

    e9_printf("Welcome to the multiboot2 test kernel: ");
    e9_printf("\t size=%d", mb_info_addr->size);
    e9_printf("\t reserved=%d", mb_info_addr->reserved);

    e9_print("\nTags:\n");

    size_t add_size = 0;

    // NOTE: We set i to 8 to skip size and reserved fields:
    for (size_t i = 8; i < mb_info_addr->size; i += add_size) { 
        struct multiboot_tag *tag = (struct multiboot_tag *)((uint8_t *)mb_info_addr + i);

        if (tag->type == MULTIBOOT_TAG_TYPE_END) {
            break;
        }

        switch (tag->type) {
            case MULTIBOOT_TAG_TYPE_CMDLINE: {
                struct multiboot_tag_string *cmdline = (struct multiboot_tag_string *)tag;
                e9_printf("\t cmdline:");
                e9_printf("\t\t string=%s", cmdline->string);
                break;
            }

            case MULTIBOOT_TAG_TYPE_BOOT_LOADER_NAME: {
                struct multiboot_tag_string *name = (struct multiboot_tag_string *)tag;
                e9_printf("\t bootloader_name:");
                e9_printf("\t\t string=%s", name->string);
                break;
            }

            case MULTIBOOT_TAG_TYPE_MODULE: {
                struct multiboot_tag_module *module = (struct multiboot_tag_module *)tag;
                e9_printf("\t module:");
                e9_printf("\t\t mod_start=%x", module->mod_start);
                e9_printf("\t\t mod_end=%x", module->mod_end);
                e9_printf("\t\t cmdline=%s", module->cmdline);
                break;
            }

            case MULTIBOOT_TAG_TYPE_BASIC_MEMINFO: {
                struct multiboot_tag_basic_meminfo *meminfo = (struct multiboot_tag_basic_meminfo *)tag;
                e9_printf("\t basic_meminfo:");
                e9_printf("\t\t mem_lower=%x", meminfo->mem_lower);
                e9_printf("\t\t mem_upper=%x", meminfo->mem_upper);
                break;
            }

            // unimplemented(Andy-Python-Programmer): MULTIBOOT_TAG_TYPE_BOOTDEV

            case MULTIBOOT_TAG_TYPE_MMAP: {
                struct multiboot_tag_mmap *mmap = (struct multiboot_tag_mmap *)tag;
                e9_printf("\t mmap:");
                e9_printf("\t\t entry_size=%d", mmap->entry_size);
                e9_printf("\t\t entry_version=%d", mmap->entry_version);
                e9_printf("\t\t useable_entries:");

                struct multiboot_mmap_entry *start = (struct multiboot_mmap_entry *)(mmap->entries);
                struct multiboot_mmap_entry *end = (struct multiboot_mmap_entry *)(mmap->entries + mmap->size);

                size_t total_mem = 0;

                // For now we only print the useable memory map entries since
                // printing the whole memory map blows my terminal up. We also
                // iterate through the avaliable memory map entries and add up
                // to find the total amount of useable memory.
                for (struct multiboot_mmap_entry* entry = start; entry < end; entry++) {
                    if (entry->type != MULTIBOOT_MEMORY_AVAILABLE) {
                        continue;
                    }

                    e9_printf("\t\t\t addr=%x", entry->addr);
                    e9_printf("\t\t\t len=%x", entry->len);
                    e9_printf("\t\t\t type=%x", entry->type);

                    total_mem += entry->len;
                }

                e9_printf("Total usable memory: %x", total_mem);

                break;
            }
        
            // unimplemented(Andy-Python-Programmer): MULTIBOOT_TAG_TYPE_VBE

            case MULTIBOOT_TAG_TYPE_FRAMEBUFFER: {
                struct multiboot_tag_framebuffer *fb = (struct multiboot_tag_framebuffer *)tag;

                e9_printf("\t framebuffer:");
                e9_printf("\t\t framebuffer_pitch: %x", fb->common.framebuffer_pitch);
                e9_printf("\t\t framebuffer_width: %x", fb->common.framebuffer_width);
                e9_printf("\t\t framebuffer_height: %x", fb->common.framebuffer_height);
                e9_printf("\t\t framebuffer_bpp: %x", fb->common.framebuffer_bpp);
                e9_printf("\t\t framebuffer_type: %x", fb->common.framebuffer_type);
                e9_printf("\t\t framebuffer_adddress: %x", fb->common.framebuffer_addr);

                switch (fb->common.framebuffer_type) {
                    case MULTIBOOT_FRAMEBUFFER_TYPE_RGB: {
                        e9_printf("\t\t framebuffer_red_field_position: %x", fb->framebuffer_red_field_position);
                        e9_printf("\t\t framebuffer_red_mask_size: %x", fb->framebuffer_red_mask_size);
                        e9_printf("\t\t framebuffer_green_field_position: %x", fb->framebuffer_green_field_position);
                        e9_printf("\t\t framebuffer_green_mask_size: %x", fb->framebuffer_green_mask_size);
                        e9_printf("\t\t framebuffer_blue_field_position: %x", fb->framebuffer_blue_field_position);
                        e9_printf("\t\t framebuffer_blue_mask_size: %x", fb->framebuffer_blue_mask_size);
                        break;
                    }

                    // Rest are unimplemented(Andy-Python-Programmer):
                }

                break;
            }
        }

        add_size = tag->size;

        // Align the size to 8 bytes.
        if ((add_size % 8) != 0) 
			add_size += (8 - add_size % 8);
    }

out:
    for (;;);
}
