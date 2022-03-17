#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <e9print.h>

static void limine_main(void);

__attribute__((used))
static struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,
    .flags = 0, .response = NULL
};

__attribute__((used))
static struct limine_entry_point_request entry_point_request = {
    .id = LIMINE_ENTRY_POINT_REQUEST,
    .flags = 0, .response = NULL,

    .entry = limine_main
};

__attribute__((used))
static struct limine_boot_info_request boot_info_request = {
    .id = LIMINE_BOOT_INFO_REQUEST,
    .flags = 0, .response = NULL
};

__attribute__((used))
static struct limine_memmap_request memmap_request = {
    .id = LIMINE_MEMMAP_REQUEST,
    .flags = 0, .response = NULL
};

__attribute__((used))
static struct limine_module_request module_request = {
    .id = LIMINE_MODULE_REQUEST,
    .flags = 0, .response = NULL
};

__attribute__((used))
static struct limine_rsdp_request rsdp_request = {
    .id = LIMINE_RSDP_REQUEST,
    .flags = 0, .response = NULL
};

__attribute__((used))
static struct limine_smbios_request smbios_request = {
    .id = LIMINE_SMBIOS_REQUEST,
    .flags = 0, .response = NULL
};

static char *get_memmap_type(uint64_t type) {
    switch (type) {
        case LIMINE_MEMMAP_USABLE:
            return "Usable";
        case LIMINE_MEMMAP_RESERVED:
            return "Reserved";
        case LIMINE_MEMMAP_ACPI_RECLAIMABLE:
            return "ACPI reclaimable";
        case LIMINE_MEMMAP_ACPI_NVS:
            return "ACPI NVS";
        case LIMINE_MEMMAP_BAD_MEMORY:
            return "Bad memory";
        case LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE:
            return "Bootloader reclaimable";
        case LIMINE_MEMMAP_KERNEL_AND_MODULES:
            return "Kernel and modules";
        case LIMINE_MEMMAP_FRAMEBUFFER:
            return "Framebuffer";
        default:
            return "???";
    }
}

static void print_file_loc(struct limine_file_location *file_location) {
    e9_printf("Loc->PartIndex: %d", file_location->partition_index);
    e9_printf("Loc->MBRDiskId: %x", file_location->mbr_disk_id);
    e9_printf("Loc->GPTDiskUUID: %x-%x-%x-%x",
              file_location->gpt_disk_uuid.a,
              file_location->gpt_disk_uuid.b,
              file_location->gpt_disk_uuid.c,
              *(uint64_t *)file_location->gpt_disk_uuid.d);
    e9_printf("Loc->GPTPartUUID: %x-%x-%x-%x",
              file_location->gpt_part_uuid.a,
              file_location->gpt_part_uuid.b,
              file_location->gpt_part_uuid.c,
              *(uint64_t *)file_location->gpt_part_uuid.d);
    e9_printf("Loc->PartUUID: %x-%x-%x-%x",
              file_location->part_uuid.a,
              file_location->part_uuid.b,
              file_location->part_uuid.c,
              *(uint64_t *)file_location->part_uuid.d);
}

#define FEAT_START do {
#define FEAT_END } while (0);

static void limine_main(void) {
    e9_printf("We're alive");

FEAT_START
    if (boot_info_request.response == NULL) {
        e9_printf("Boot info not passed");
        break;
    }
    struct limine_boot_info_response *boot_info_response = boot_info_request.response;
    e9_printf("Boot info response:");
    e9_printf("Bootloader name: %s", boot_info_response->loader);
FEAT_END

FEAT_START
    if (memmap_request.response == NULL) {
        e9_printf("Memory map not passed");
        break;
    }
    struct limine_memmap_response *memmap_response = memmap_request.response;
    e9_printf("%d memory map entries", memmap_response->entries_count);
    for (size_t i = 0; i < memmap_response->entries_count; i++) {
        struct limine_memmap_entry *e = &memmap_response->entries[i];
        e9_printf("%x->%x %s", e->base, e->base + e->length, get_memmap_type(e->type));
    }
FEAT_END

FEAT_START
    if (framebuffer_request.response == NULL) {
        e9_printf("Framebuffer not passed");
        break;
    }
    struct limine_framebuffer_response *fb_response = framebuffer_request.response;
    e9_printf("%d framebuffer(s)", fb_response->fbs_count);
    for (size_t i = 0; i < fb_response->fbs_count; i++) {
        struct limine_framebuffer *fb = &fb_response->fbs[i];
        e9_printf("Address: %x", fb->address);
        e9_printf("Width: %d", fb->width);
        e9_printf("Height: %d", fb->height);
        e9_printf("Pitch: %d", fb->pitch);
        e9_printf("BPP: %d", fb->bpp);
        e9_printf("Memory model: %d", fb->memory_model);
        e9_printf("Red mask size: %d", fb->red_mask_size);
        e9_printf("Red mask shift: %d", fb->red_mask_shift);
        e9_printf("Green mask size: %d", fb->green_mask_size);
        e9_printf("Green mask shift: %d", fb->green_mask_shift);
        e9_printf("Blue mask size: %d", fb->blue_mask_size);
        e9_printf("Blue mask shift: %d", fb->blue_mask_shift);
    }
FEAT_END

FEAT_START
    if (module_request.response == NULL) {
        e9_printf("Modules not passed");
        break;
    }
    struct limine_module_response *module_response = module_request.response;
    e9_printf("%d module(s)", module_response->modules_count);
    for (size_t i = 0; i < module_response->modules_count; i++) {
        struct limine_module *m = &module_response->modules[i];

        e9_printf("Base: %x", m->base);
        e9_printf("Length: %x", m->length);
        e9_printf("Path: %s", m->path);
        e9_printf("Cmdline: %s", m->cmdline);

        print_file_loc(m->file_location);
    }
FEAT_END

FEAT_START
    if (rsdp_request.response == NULL) {
        e9_printf("RSDP not passed");
        break;
    }
    struct limine_rsdp_response *rsdp_response = rsdp_request.response;
    e9_printf("RSDP at: %x", rsdp_response->address);
FEAT_END

FEAT_START
    if (smbios_request.response == NULL) {
        e9_printf("SMBIOS not passed");
        break;
    }
    struct limine_smbios_response *smbios_response = smbios_request.response;
    e9_printf("SMBIOS 32-bit entry at: %x", smbios_response->entry_32);
    e9_printf("SMBIOS 64-bit entry at: %x", smbios_response->entry_64);
FEAT_END

    for (;;);
}
