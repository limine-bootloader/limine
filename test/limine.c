#include <stdint.h>
#include <stddef.h>
#include <limine.h>
#include <e9print.h>

static struct limine_framebuffer_request framebuffer_request = {
    .id = LIMINE_FRAMEBUFFER_REQUEST,

    .flags = LIMINE_FRAMEBUFFER_PREFER_LFB | LIMINE_FRAMEBUFFER_ENFORCE_PREFER,

    .height = 0, .width = 0, .bpp = 0
};

static void *features_array[] = {
    LIMINE_BOOT_INFO_REQUEST,
    &framebuffer_request,
    LIMINE_MEMMAP_REQUEST,
    LIMINE_5_LEVEL_PAGING_REQUEST,
    LIMINE_PMR_REQUEST
};

static void limine_main(void);

__attribute__((used, aligned(16)))
static struct limine_header limine_header = {
    .magic = LIMINE_MAGIC,
    .entry = limine_main,
    .features_count = sizeof(features_array) / sizeof(void *),
    .features = features_array
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

#define FEAT_START do {
#define FEAT_END } while (0);

static void limine_main(void) {
    e9_printf("We're alive");

FEAT_START
    if (features_array[0] == NULL) {
        e9_printf("Boot info not passed");
        break;
    }
    struct limine_boot_info_response *boot_info_response = features_array[0];
    e9_printf("Boot info response:");
    e9_printf("Bootloader name: %s", boot_info_response->loader);
FEAT_END

FEAT_START
    if (features_array[2] == NULL) {
        e9_printf("Memory map not passed");
        break;
    }
    struct limine_memmap_response *memmap_response = features_array[2];
    e9_printf("%d memory map entries", memmap_response->entries_count);
    for (size_t i = 0; i < memmap_response->entries_count; i++) {
        struct limine_memmap_entry *e = &memmap_response->entries[i];
        e9_printf("%x->%x %s", e->base, e->base + e->length, get_memmap_type(e->type));
    }
FEAT_END

    for (;;);
}
