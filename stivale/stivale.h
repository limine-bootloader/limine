#ifndef __STIVALE__STIVALE_H__
#define __STIVALE__STIVALE_H__

#include <stdint.h>

/* --- Header --------------------------------------------------------------- */
/*  Information passed from the kernel to the bootloader                      */

struct stivale_header {
    uint64_t stack;
    uint16_t flags;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
    uint64_t entry_point;
} __attribute__((packed));

/* --- Struct --------------------------------------------------------------- */
/*  Information passed from the bootloader to the kernel                      */

struct stivale_module {
    uint64_t begin;
    uint64_t end;
    char string[128];
    uint64_t next;
} __attribute__((packed));

enum  {
    STIVALE_MMAP_USABLE = 1,
    STIVALE_MMAP_RESERVED = 2,
    STIVALE_MMAP_ACPI_RECLAIMABLE = 3,
    STIVALE_MMAP_ACPI_NVS = 4,
    STIVALE_MMAP_BAD_MEMORY = 5,
    STIVALE_MMAP_KERNEL_AND_MODULES = 10, // Kernel/Modules
};

struct stivale_mmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t unused;
} __attribute__((packed));

struct stivale_struct {
    uint64_t cmdline;
    uint64_t memory_map_addr;
    uint64_t memory_map_entries;
    uint64_t framebuffer_addr;
    uint16_t framebuffer_pitch;
    uint16_t framebuffer_width;
    uint16_t framebuffer_height;
    uint16_t framebuffer_bpp;
    uint64_t rsdp;
    uint64_t module_count;
    uint64_t modules;
    uint64_t epoch;
    uint64_t flags; // bit 0: 1 if booted with BIOS, 0 if booted with UEFI
} __attribute__((packed));

#endif
