#ifndef _LIMINE_H
#define _LIMINE_H 1

#include <stdint.h>

// Misc

#ifdef LIMINE_NO_POINTERS
#  define LIMINE_PTR(TYPE) uint64_t
#else
#  define LIMINE_PTR(TYPE) TYPE
#endif

#define LIMINE_COMMON_MAGIC 0xc7b1dd30df4c8b88, 0x0a82e883a194f07b

struct limine_uuid {
    uint32_t a;
    uint16_t b;
    uint16_t c;
    uint8_t d[8];
};

struct limine_file_location {
    uint64_t partition_index;
    uint32_t pxe_ip;
    uint32_t pxe_port;
    uint32_t mbr_disk_id;
    struct limine_uuid gpt_disk_uuid;
    struct limine_uuid gpt_part_uuid;
    struct limine_uuid part_uuid;
};

// Boot info

#define LIMINE_BOOT_INFO_REQUEST { LIMINE_COMMON_MAGIC, 0xf55038d8e2a1202f, 0x279426fcf5f59740 }

struct limine_boot_info_response {
    uint64_t flags;
    LIMINE_PTR(char *) loader;
    LIMINE_PTR(void *) hhdm;
};

struct limine_boot_info_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_boot_info_response *) response;
};

// Framebuffer

#define LIMINE_FRAMEBUFFER_REQUEST { LIMINE_COMMON_MAGIC, 0xcbfe81d7dd2d1977, 0x063150319ebc9b71 }

#define LIMINE_FRAMEBUFFER_RGB 1

struct limine_framebuffer {
    LIMINE_PTR(void *) address;
    uint16_t width;
    uint16_t height;
    uint16_t pitch;
    uint16_t bpp;
    uint8_t memory_model;
    uint8_t red_mask_size;
    uint8_t red_mask_shift;
    uint8_t green_mask_size;
    uint8_t green_mask_shift;
    uint8_t blue_mask_size;
    uint8_t blue_mask_shift;
    uint8_t unused;
    uint8_t reserved[256];
};

struct limine_framebuffer_response {
    uint64_t flags;

    uint64_t fbs_count;
    LIMINE_PTR(struct limine_framebuffer *) fbs;
};

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

// SMP

#define LIMINE_SMP_REQUEST { LIMINE_COMMON_MAGIC, 0x95a67b819a1b857e, 0xa0b61b723b6a73e0 }

#define LIMINE_SMP_X2APIC (1 << 0)

struct limine_smp_info {
    uint32_t processor_id;
    uint32_t lapic_id;
    uint64_t reserved;
    LIMINE_PTR(void *) goto_address;
    uint64_t extra_argument;
};

struct limine_smp_response {
    uint64_t flags;

    uint32_t bsp_lapic_id;
    uint32_t unused;
    uint64_t cpus_count;
    LIMINE_PTR(struct limine_smp_info *) cpus;
};

struct limine_smp_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_smp_response *) response;
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
    uint8_t reserved[256];
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

// Module

#define LIMINE_MODULE_REQUEST { LIMINE_COMMON_MAGIC, 0x3e7e279702be32af, 0xca1c4f3bd1280cee }

struct limine_module {
    uint64_t base;
    uint64_t length;
    LIMINE_PTR(char *) path;
    LIMINE_PTR(char *) cmdline;
    LIMINE_PTR(struct limine_file_location *) file_location;
    uint8_t reserved[256];
};

struct limine_module_response {
    uint64_t flags;

    uint64_t modules_count;
    LIMINE_PTR(struct limine_module *) modules;
};

struct limine_module_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_module_response *) response;
};

// RSDP

#define LIMINE_RSDP_REQUEST { LIMINE_COMMON_MAGIC, 0xc5e77b6b397e7b43, 0x27637845accdcf3c }

struct limine_rsdp_response {
    uint64_t flags;

    LIMINE_PTR(void *) address;
};

struct limine_rsdp_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_rsdp_response *) response;
};

// SMBIOS

#define LIMINE_SMBIOS_REQUEST { LIMINE_COMMON_MAGIC, 0x9e9046f11e095391, 0xaa4a520fefbde5ee }

struct limine_smbios_response {
    uint64_t flags;

    LIMINE_PTR(void *) entry_32;
    LIMINE_PTR(void *) entry_64;
};

struct limine_smbios_request {
    uint64_t id[4];
    uint64_t flags;
    LIMINE_PTR(struct limine_smbios_response *) response;
};

#endif
