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

#define MAX_REQUESTS 128
#define MAX_MEMMAP 256

static uint64_t physical_base, virtual_base, slide, direct_map_offset;
static size_t requests_count;
static void *requests[MAX_REQUESTS];

static struct limine_file_location get_file_loc(struct volume *vol) {
    struct limine_file_location ret = {0};

    ret.partition_index = vol->partition;

    ret.mbr_disk_id = mbr_get_id(vol);

    if (vol->guid_valid) {
        memcpy(&ret.part_uuid, &vol->guid, sizeof(struct limine_uuid));
    }

    if (vol->part_guid_valid) {
        memcpy(&ret.gpt_part_uuid, &vol->part_guid, sizeof(struct limine_uuid));
    }

    struct guid gpt_disk_uuid;
    if (gpt_get_guid(&gpt_disk_uuid, vol->backing_dev ?: vol) == true) {
        memcpy(&ret.gpt_disk_uuid, &gpt_disk_uuid, sizeof(struct limine_uuid));
    }

    return ret;
}

static uint64_t reported_addr(void *addr) {
    return (uint64_t)(uintptr_t)addr + direct_map_offset;
}

/*
static uintptr_t get_phys_addr(uint64_t addr) {
    return physical_base + (addr - virtual_base);
}
*/

static void *_get_request(uint64_t id[4]) {
    for (size_t i = 0; i < requests_count; i++) {
        uint64_t *p = requests[i];

        if (p[2] != id[2]) {
            continue;
        }
        if (p[3] != id[3]) {
            continue;
        }

        return p;
    }

    return NULL;
}

#define get_request(REQ) _get_request((uint64_t[4])REQ)

#define FEAT_START do {
#define FEAT_END } while (0);

bool limine_load(char *config, char *cmdline) {
    (void)cmdline;

    uint32_t eax, ebx, ecx, edx;

    char *kernel_path = config_get_value(config, 0, "KERNEL_PATH");
    if (kernel_path == NULL)
        panic(true, "limine: KERNEL_PATH not specified");

    struct file_handle *kernel_file;
    if ((kernel_file = uri_open(kernel_path)) == NULL)
        panic(true, "limine: Failed to open kernel with path `%s`. Is the path correct?", kernel_path);

    uint8_t *kernel = freadall(kernel_file, MEMMAP_BOOTLOADER_RECLAIMABLE);

    size_t kernel_file_size = kernel_file->size;

    //struct volume *kernel_volume = kernel_file->vol;

    fclose(kernel_file);

    char *kaslr_s = config_get_value(config, 0, "KASLR");
    bool kaslr = true;
    if (kaslr_s != NULL && strcmp(kaslr_s, "no") == 0)
        kaslr = false;

    int bits = elf_bits(kernel);

    if (bits == -1 || bits == 32) {
        printv("limine: Kernel in unrecognised format");
        return false;
    }

    // ELF loading
    uint64_t entry_point = 0;
    struct elf_range *ranges;
    uint64_t ranges_count;

    if (elf64_load(kernel, &entry_point, NULL, &slide,
                   MEMMAP_KERNEL_AND_MODULES, kaslr, false,
                   &ranges, &ranges_count,
                   true, &physical_base, &virtual_base)) {
        return false;
    }

    // Load requests
    requests_count = 0;
    uint64_t common_magic[2] = { LIMINE_COMMON_MAGIC };
    for (size_t i = 0; i < ALIGN_DOWN(kernel_file_size, 8); i += 8) {
        uint64_t *p = (void *)(uintptr_t)physical_base + i;

        if (p[0] != common_magic[0]) {
            continue;
        }
        if (p[1] != common_magic[1]) {
            continue;
        }

        if (requests_count == MAX_REQUESTS) {
            panic(true, "limine: Maximum requests exceeded");
        }

        // Check for a conflict
        if (_get_request(p) != NULL) {
            panic(true, "limine: Conflict detected for request ID %X %X", p[2], p[3]);
        }

        requests[requests_count++] = p;
    }

    if (requests_count == 0) {
        return false;
    }

    // Check if 64 bit CPU
    if (!cpuid(0x80000001, 0, &eax, &ebx, &ecx, &edx) || !(edx & (1 << 29))) {
        panic(true, "limine: This CPU does not support 64-bit mode.");
    }

    print("limine: Loading kernel `%s`...\n", kernel_path);

    printv("limine: Physical base:   %X\n", physical_base);
    printv("limine: Virtual base:    %X\n", virtual_base);
    printv("limine: Slide:           %X\n", slide);
    printv("limine: ELF entry point: %X\n", entry_point);
    printv("limine: Requests count:  %u\n", requests_count);

    // 5 level paging feature & HHDM slide
    bool want_5lv;
FEAT_START
    // Check if 5-level paging is available
    bool level5pg = false;
    if (cpuid(0x00000007, 0, &eax, &ebx, &ecx, &edx) && (ecx & (1 << 16))) {
        printv("limine: CPU has 5-level paging support\n");
        level5pg = true;
    }

    struct limine_5_level_paging_request *lv5pg_request = get_request(LIMINE_5_LEVEL_PAGING_REQUEST);
    want_5lv = lv5pg_request != NULL && level5pg;

    direct_map_offset = want_5lv ? 0xff00000000000000 : 0xffff800000000000;

    if (kaslr) {
        direct_map_offset += (rand64() & ~((uint64_t)0x40000000 - 1)) & 0xfffffffffff;
    }

    if (want_5lv) {
        void *lv5pg_response = ext_mem_alloc(sizeof(struct limine_5_level_paging_response));
        lv5pg_request->response = reported_addr(lv5pg_response);
    }
FEAT_END

    // Entry point feature
FEAT_START
    struct limine_entry_point_request *entrypoint_request = get_request(LIMINE_ENTRY_POINT_REQUEST);
    if (entrypoint_request == NULL) {
        break;
    }

    entry_point = entrypoint_request->entry;

    print("limine: Entry point at %X\n", entry_point);

    struct limine_entry_point_response *entrypoint_response =
        ext_mem_alloc(sizeof(struct limine_entry_point_response));

    entrypoint_request->response = reported_addr(entrypoint_response);
FEAT_END

    // Boot info feature
FEAT_START
    struct limine_boot_info_request *boot_info_request = get_request(LIMINE_BOOT_INFO_REQUEST);
    if (boot_info_request == NULL) {
        break; // next feature
    }

    struct limine_boot_info_response *boot_info_response =
        ext_mem_alloc(sizeof(struct limine_boot_info_response));

    boot_info_response->loader = reported_addr("Limine " LIMINE_VERSION);

    boot_info_request->response = reported_addr(boot_info_response);
FEAT_END

    // Command line
FEAT_START
    struct limine_cmdline_request *cmdline_request = get_request(LIMINE_CMDLINE_REQUEST);
    if (cmdline_request == NULL) {
        break; // next feature
    }

    struct limine_cmdline_response *cmdline_response =
        ext_mem_alloc(sizeof(struct limine_cmdline_response));

    cmdline_response->cmdline = reported_addr(cmdline);

    cmdline_request->response = reported_addr(cmdline_response);
FEAT_END

    // Modules
FEAT_START
    struct limine_module_request *module_request = get_request(LIMINE_MODULE_REQUEST);
    if (module_request == NULL) {
        break; // next feature
    }

    size_t module_count;
    for (module_count = 0; ; module_count++) {
        char *module_file = config_get_value(config, module_count, "MODULE_PATH");
        if (module_file == NULL)
            break;
    }

    struct limine_module_response *module_response =
        ext_mem_alloc(sizeof(struct limine_module_response));

    struct limine_module *modules = ext_mem_alloc(module_count * sizeof(struct limine_module));

    for (size_t i = 0; i < module_count; i++) {
        struct conf_tuple conf_tuple =
                config_get_tuple(config, i, "MODULE_PATH", "MODULE_CMDLINE");

        char *module_path = conf_tuple.value1;
        char *module_cmdline = conf_tuple.value2;

        struct limine_module *m = &modules[i];

        if (module_cmdline == NULL) {
            module_cmdline = "";
        }

        print("limine: Loading module `%s`...\n", module_path);

        struct file_handle *f;
        if ((f = uri_open(module_path)) == NULL)
            panic(true, "limine: Failed to open module with path `%s`. Is the path correct?", module_path);

        m->base = reported_addr(freadall(f, MEMMAP_KERNEL_AND_MODULES));
        m->length = f->size;
        m->path = reported_addr(module_path);
        m->cmdline = reported_addr(module_cmdline);

        struct limine_file_location *l = ext_mem_alloc(sizeof(struct limine_file_location));
        *l = get_file_loc(f->vol);

        m->file_location = reported_addr(l);

        fclose(f);
    }

    module_response->modules_count = module_count;
    module_response->modules = reported_addr(modules);

    module_request->response = reported_addr(module_response);
FEAT_END

    // Framebuffer feature
FEAT_START
    term_deinit();

    size_t req_width = 0, req_height = 0, req_bpp = 0;

    char *resolution = config_get_value(config, 0, "RESOLUTION");
    if (resolution != NULL) {
        parse_resolution(&req_width, &req_height, &req_bpp, resolution);
    }

    struct fb_info fb;

    if (!fb_init(&fb, req_width, req_height, req_bpp)) {
        panic(true, "limine: Could not acquire framebuffer");
    }

    struct limine_framebuffer_request *framebuffer_request = get_request(LIMINE_FRAMEBUFFER_REQUEST);
    if (framebuffer_request == NULL) {
        break; // next feature
    }

    memmap_alloc_range(fb.framebuffer_addr,
                       (uint64_t)fb.framebuffer_pitch * fb.framebuffer_height,
                       MEMMAP_FRAMEBUFFER, false, false, false, true);

    struct limine_framebuffer_response *framebuffer_response =
        ext_mem_alloc(sizeof(struct limine_framebuffer_response));

    // For now we only support 1 framebuffer
    struct limine_framebuffer *fbp = ext_mem_alloc(sizeof(struct limine_framebuffer));
    framebuffer_response->fbs = reported_addr(fbp);
    framebuffer_response->fbs_count = 1;

    fbp->memory_model     = LIMINE_FRAMEBUFFER_RGB;
    fbp->address          = reported_addr((void *)(uintptr_t)fb.framebuffer_addr);
    fbp->width            = fb.framebuffer_width;
    fbp->height           = fb.framebuffer_height;
    fbp->bpp              = fb.framebuffer_bpp;
    fbp->pitch            = fb.framebuffer_pitch;
    fbp->red_mask_size    = fb.red_mask_size;
    fbp->red_mask_shift   = fb.red_mask_shift;
    fbp->green_mask_size  = fb.green_mask_size;
    fbp->green_mask_shift = fb.green_mask_shift;
    fbp->blue_mask_size   = fb.blue_mask_size;
    fbp->blue_mask_shift  = fb.blue_mask_shift;

    framebuffer_request->response = reported_addr(framebuffer_response);
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
    struct limine_memmap_request *memmap_request = get_request(LIMINE_MEMMAP_REQUEST);
    struct limine_memmap_response *memmap_response;
    struct limine_memmap_entry *_memmap;

    if (memmap_request != NULL) {
        memmap_response = ext_mem_alloc(sizeof(struct limine_memmap_response));
        _memmap = ext_mem_alloc(sizeof(struct limine_memmap_entry) * MAX_MEMMAP);
    }

    size_t mmap_entries;
    struct e820_entry_t *mmap = get_memmap(&mmap_entries);

    if (memmap_request == NULL) {
        break; // next feature
    }

    if (mmap_entries > MAX_MEMMAP) {
        panic(false, "limine: Too many memmap entries");
    }

    for (size_t i = 0; i < mmap_entries; i++) {
        _memmap[i].base = mmap[i].base;
        _memmap[i].length = mmap[i].length;

        switch (mmap[i].type) {
            case MEMMAP_USABLE:
                _memmap[i].type = LIMINE_MEMMAP_USABLE;
                break;
            case MEMMAP_ACPI_RECLAIMABLE:
                _memmap[i].type = LIMINE_MEMMAP_ACPI_RECLAIMABLE;
                break;
            case MEMMAP_ACPI_NVS:
                _memmap[i].type = LIMINE_MEMMAP_ACPI_NVS;
                break;
            case MEMMAP_BAD_MEMORY:
                _memmap[i].type = LIMINE_MEMMAP_BAD_MEMORY;
                break;
            case MEMMAP_BOOTLOADER_RECLAIMABLE:
                _memmap[i].type = LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE;
                break;
            case MEMMAP_KERNEL_AND_MODULES:
                _memmap[i].type = LIMINE_MEMMAP_KERNEL_AND_MODULES;
                break;
            case MEMMAP_FRAMEBUFFER:
                _memmap[i].type = LIMINE_MEMMAP_FRAMEBUFFER;
                break;
            default:
            case MEMMAP_RESERVED:
                _memmap[i].type = LIMINE_MEMMAP_RESERVED;
                break;
        }
    }

    memmap_response->entries_count = mmap_entries;
    memmap_response->entries = reported_addr(_memmap);

    memmap_request->response = reported_addr(memmap_response);
FEAT_END

    // Final wrap-up
#if uefi == 1
    efi_exit_boot_services();
#endif

    stivale_spinup(64, want_5lv, &pagemap, entry_point, 0,
                   reported_addr(stack), true, (uintptr_t)local_gdt);

    __builtin_unreachable();
}
