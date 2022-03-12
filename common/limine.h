#ifndef _LIMINE_H
#define _LIMINE_H 1

#include <stdint.h>

#ifdef LIMINE_NO_POINTERS
#  define LIMINE_PTR(TYPE) uint64_t
#else
#  define LIMINE_PTR(TYPE) TYPE
#endif

#define LIMINE_MAGIC { 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b }

struct limine_header {
    uint64_t magic[2];
    LIMINE_PTR(void *) entry;
    uint64_t features_count;
    LIMINE_PTR(void *) features;
};

// Boot info

#define LIMINE_BOOT_INFO_REQUEST ((LIMINE_PTR(void *)) 1 )

struct limine_boot_info_response {
    uint64_t flags;
    LIMINE_PTR(char *) loader;
};

// Framebuffer

#define LIMINE_FRAMEBUFFER_REQUEST ((LIMINE_PTR(void *)) 2 )

struct limine_framebuffer_request {
    LIMINE_PTR(void *) id;

#define LIMINE_FRAMEBUFFER_PREFER_LFB 0
#define LIMINE_FRAMEBUFFER_PREFER_TEXT 1
#define LIMINE_FRAMEBUFFER_ENFORCE_PREFER (1 << 8)
    uint64_t flags;

    uint16_t width;
    uint16_t height;
    uint16_t bpp;

    uint16_t unused;
};

// 5-level paging

#define LIMINE_5_LEVEL_PAGING_REQUEST ((LIMINE_PTR(void *)) 3 )

struct limine_5_level_paging_response {
    uint64_t flags;
};

// PMRs

#define LIMINE_PMR_REQUEST ((LIMINE_PTR(void *)) 4 )

// Memory map

#define LIMINE_MEMMAP_REQUEST ((LIMINE_PTR(void *)) 5 )

#define LIMINE_MEMMAP_USABLE                 0
#define LIMINE_MEMMAP_RESERVED               1
#define LIMINE_MEMMAP_ACPI_RECLAIMABLE       2
#define LIMINE_MEMMAP_ACPI_NVS               3
#define LIMINE_MEMMAP_BAD_MEMORY             4
#define LIMINE_MEMMAP_BOOTLOADER_RECLAIMABLE 5
#define LIMINE_MEMMAP_KERNEL_AND_MODULES     6
#define LIMINE_MEMMAP_FRAMEBUFFER            7

struct limine_memmap_entry {
    uint64_t base;
    uint64_t length;
    uint64_t type;
};

struct limine_memmap_response {
    uint64_t flags;

    uint64_t entries_count;
    LIMINE_PTR(struct limine_memmap_entry *) entries;
};

#endif
