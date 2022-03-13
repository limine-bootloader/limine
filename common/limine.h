#ifndef _LIMINE_H
#define _LIMINE_H 1

#include <stdint.h>

#ifdef LIMINE_NO_POINTERS
#  define LIMINE_PTR(TYPE) uint64_t
#else
#  define LIMINE_PTR(TYPE) TYPE
#endif

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

// Boot info

#define LIMINE_BOOT_INFO_REQUEST { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202f, 0x279426fcf5f59740 }

struct limine_boot_info_response {
    uint64_t flags;
    LIMINE_PTR(char *) loader;
};

struct limine_boot_info_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_boot_info_response *) response;
};

// Framebuffer

#define LIMINE_FRAMEBUFFER_REQUEST { LIMINE_COMMON_MAGIC, 0xcbfe81d7dd2d1977, 0x063150319ebc9b71 }

struct limine_framebuffer_response {
    uint64_t flags;
};

#define LIMINE_FRAMEBUFFER_PREFER_LFB 0
#define LIMINE_FRAMEBUFFER_PREFER_TEXT 1
#define LIMINE_FRAMEBUFFER_ENFORCE_PREFER (1 << 8)

struct limine_framebuffer_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_framebuffer_response *) response;
};

// 5-level paging

#define LIMINE_5_LEVEL_PAGING_REQUEST { LIMINE_COMMON_MAGIC, 0x94469551da9b3192, 0xebe5e86db7382888 }

struct limine_5_level_paging_response {
    uint64_t flags;
};

struct limine_5_level_paging_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_5_level_paging_response *) response;
};

// Memory map

#define LIMINE_MEMMAP_REQUEST { LIMINE_COMMON_MAGIC, 0x67cf3d9d378a806f, 0xe304acdfc50c3c62 }

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

struct limine_memmap_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_memmap_response *) response;
};

// Entry point

#define LIMINE_ENTRY_POINT_REQUEST { LIMINE_COMMON_MAGIC, 0x13d86c035a1cd3e1, 0x2b0caa89d8f3026a }

struct limine_entry_point_response {
    uint64_t flags;
};

struct limine_entry_point_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_entry_point_response *) response;

    LIMINE_PTR(void *) entry;
};

#endif
