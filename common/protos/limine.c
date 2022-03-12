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
#include <lib/rand.h>
#define LIMINE_NO_POINTERS
#include <protos/limine.h>
#include <limine.h>

static uint64_t features_count, physical_base, virtual_base, slide, direct_map_offset;
static uint64_t *features, *features_orig;

static uint64_t reported_addr(void *addr) {
    return (uint64_t)(uintptr_t)addr + direct_map_offset;
}

static uintptr_t get_phys_addr(uint64_t addr) {
    return physical_base + (addr - virtual_base);
}

struct feature {
    bool found;
    size_t index;
    void *request;
};

static struct feature get_feature(uint64_t id) {
    for (size_t i = 0; i < features_count; i++) {
        if (features[i] < 0xffffffff80000000 && features[i] == id) {
            return (struct feature){
                .found = true,
                .index = i,
                .request = NULL
            };
        } else {
            uint64_t *id_ptr = (void *)get_phys_addr(features[i] + slide);
            if (*id_ptr == id) {
                return (struct feature){
                    .found = true,
                    .index = i,
                    .request = (void *)id_ptr
                };
            }
        }
    }

    return (struct feature){
        .found = false,
        .index = 0,
        .request = NULL
    };
}

#define FEAT_START do {
#define FEAT_END } while (0);

bool limine_load(char *config, char *cmdline) {
    (void)cmdline;

    uint32_t eax, ebx, ecx, edx;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "limine: KERNEL_PATH not specified");

    print("limine: Loading kernel `%s`...\n", kernel_path);

    struct file_handle *kernel_file;
    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic(true, "limine: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_BOOTLOADER_RECLAIMABLE);

    size_t kernel_file_size = kernel_file->size;

    //struct volume *kernel_volume = kernel_file->vol;

    fclose(kernel_file);

    // Search for header
    struct limine_header *limine_header = NULL;
    uint64_t limine_magic[2] = LIMINE_MAGIC;
    for (size_t i = 0; i < kernel_file_size; i += 16) {
        if (memcmp(kernel + i, limine_magic, 16) == 0) {
            limine_header = (void *)(kernel + i);
        }
    }

    if (limine_header == NULL) {
        panic(true, "limine: Magic number not found");
    }

    printv("limine: Header found at %p\n", (size_t)limine_header - (size_t)kernel);

    // Check if 64 bit CPU
    if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
        panic(true, "limine: This CPU does not support 64-bit mode.");
    }

    char *kaslr_s = config_get_value(config, 0, "KASLR");
    bool kaslr = true;
    if (kaslr_s != NULL && strcmp(kaslr_s, "no") == 0)
        kaslr = false;

    int bits = elf_bits(kernel);

    if (bits == -1 || bits == 32) {
        panic(true, "limine: Kernel in unrecognised format");
    }

    uint64_t entry_point = 0;
    struct elf_range *ranges;
    uint64_t ranges_count;

    if (elf64_load(kernel, &entry_point, NULL, &slide,
                   MEMMAP_KERNEL_AND_MODULES, kaslr, false,
                   &ranges, &ranges_count,
                   true, &physical_base, &virtual_base)) {
        panic(true, "limine: ELF64 load failure");
    }

    if (limine_header->entry != 0) {
        entry_point = limine_header->entry + slide;
    }

    printv("limine: Physical base: %X\n", physical_base);
    printv("limine: Virtual base:  %X\n", virtual_base);
    printv("limine: Slide:         %X\n", slide);
    printv("limine: Entry point:   %X\n", entry_point);

    // Prepare features

    features_count = limine_header->features_count;
    features_orig = (void *)get_phys_addr(limine_header->features + slide);

    features = ext_mem_alloc(features_count * sizeof(uint64_t));
    memcpy(features, features_orig, features_count * sizeof(uint64_t));

    for (size_t i = 0; i < features_count; i++) {
        features_orig[i] = 0;
    }

    printv("limine: Features count: %U\n", features_count);
    printv("limine: Features list at %X (%p)\n", limine_header->features, features_orig);

    // 5 level paging feature & HHDM slide
    bool want_5lv;
FEAT_START
    // Check if 5-level paging is available
    bool level5pg = false;
    if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
        printv("limine: CPU has 5-level paging support\n");
        level5pg = true;
    }

    struct feature lv5pg_feat = get_feature(LIMINE_5_LEVEL_PAGING_REQUEST);
    want_5lv = lv5pg_feat.found && level5pg;

    direct_map_offset = want_5lv ? 0xff00000000000000 : 0xffff800000000000;

    if (kaslr) {
        direct_map_offset += (rand64() & ~((uint64_t)0x40000000 - 1)) & 0xfffffffffff;
    }

    if (want_5lv) {
        void *lv5pg_response = ext_mem_alloc(sizeof(struct limine_5_level_paging_response));
        features_orig[lv5pg_feat.index] = reported_addr(lv5pg_response);
    }
FEAT_END

    // Boot info feature
FEAT_START
    struct feature boot_info_feat = get_feature(LIMINE_BOOT_INFO_REQUEST);
    if (boot_info_feat.found == false) {
        break; // next feature
    }

    struct limine_boot_info_response *boot_info_response =
        ext_mem_alloc(sizeof(struct limine_boot_info_response));

    boot_info_response->loader = reported_addr("Limine " LIMINE_VERSION);

    features_orig[boot_info_feat.index] = reported_addr(boot_info_response);
FEAT_END

    // Framebuffer feature
FEAT_START
    term_deinit();

    struct fb_info *fb = NULL;
    fb_init(fb, 0, 0, 0);
FEAT_END

    // Wrap-up stuff before memmap close
    struct gdtr *local_gdt = ext_mem_alloc(sizeof(struct gdtr));
    local_gdt->limit = gdt.limit;
    uint64_t local_gdt_base = (uint64_t)gdt.ptr;
    local_gdt_base += direct_map_offset;
    local_gdt->ptr = local_gdt_base;
#if defined (__i386__)
    local_gdt->ptr_hi = local_gdt_base >> 32;
#endif

    void *stack = ext_mem_alloc(8192) + 8192;

    pagemap_t pagemap = {0};
    pagemap = stivale_build_pagemap(want_5lv, true, ranges, ranges_count, true,
                                    physical_base, virtual_base, direct_map_offset);

    // Memmap
FEAT_START
    struct feature memmap_feat = get_feature(LIMINE_MEMMAP_REQUEST);
    struct limine_memmap_response *memmap_response;

    if (memmap_feat.found == true) {
        memmap_response = ext_mem_alloc(sizeof(struct limine_memmap_response));
    }

    size_t mmap_entries;
    struct e820_entry_t *mmap = get_memmap(&mmap_entries);

    if (memmap_feat.found == false) {
        break; // next feature
    }

    for (size_t i = 0; i < mmap_entries; i++) {
        switch (mmap[i].type) {
            case MEMMAP_USABLE:
                mmap[i].type = LIMINE_MEMMAP_USABLE;
                break;
            case MEMMAP_ACPI_RECLAIMABLE:
                mmap[i].type = LIMINE_MEMMAP_ACPI_RECLAIMABLE;
                break;
            case MEMMAP_ACPI_NVS:
                mmap[i].type = LIMINE_MEMMAP_ACPI_NVS;
                break;
            case MEMMAP_BAD_MEMORY:
                mmap[i].type = LIMINE_MEMMAP_BAD_MEMORY;
                break;
            case MEMMAP_BOOTLOADER_RECLAIMABLE:
                mmap[i].type = LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE;
                break;
            case MEMMAP_KERNEL_AND_MODULES:
                mmap[i].type = LIMINE_MEMMAP_KERNEL_AND_MODULES;
                break;
            case MEMMAP_FRAMEBUFFER:
                mmap[i].type = LIMINE_MEMMAP_FRAMEBUFFER;
                break;
            default:
            case MEMMAP_RESERVED:
                mmap[i].type = LIMINE_MEMMAP_RESERVED;
                break;
        }
    }

    memmap_response->entries_count = mmap_entries;
    memmap_response->entries = reported_addr(mmap);

    features_orig[memmap_feat.index] = reported_addr(memmap_response);
FEAT_END

    // Final wrap-up
#if uefi == 1
    efi_exit_boot_services();
#endif

    stivale_spinup(64, want_5lv, &pagemap, entry_point, 0,
                   reported_addr(stack), true, (uintptr_t)local_gdt);

    __builtin_unreachable();
}
