#ifndef __MM__PMM_H__
#define __MM__PMM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

struct memmap_entry {
    uint64_t base;
    uint64_t length;
    uint32_t type;
    uint32_t unused;
};

#define MEMMAP_USABLE                 1
#define MEMMAP_RESERVED               2
#define MEMMAP_ACPI_RECLAIMABLE       3
#define MEMMAP_ACPI_NVS               4
#define MEMMAP_BAD_MEMORY             5
#define MEMMAP_BOOTLOADER_RECLAIMABLE 0x1000
#define MEMMAP_KERNEL_AND_MODULES     0x1001
#define MEMMAP_FRAMEBUFFER            0x1002
#define MEMMAP_EFI_RECLAIMABLE        0x2000
#define MEMMAP_EFI_BOOTSERVICES       0x2001

struct meminfo {
    size_t uppermem;
    size_t lowermem;
};

struct meminfo mmap_get_info(size_t mmap_count, struct memmap_entry *mmap);

#if defined (BIOS)
extern struct memmap_entry memmap[];
extern size_t memmap_entries;
#endif

#if defined (UEFI)
extern struct memmap_entry *memmap;
extern size_t memmap_entries;

extern struct memmap_entry *untouched_memmap;
extern size_t untouched_memmap_entries;
#endif

extern bool allocations_disallowed;

void init_memmap(void);
struct memmap_entry *get_memmap(size_t *entries);
struct memmap_entry *get_raw_memmap(size_t *entry_count);
void print_memmap(struct memmap_entry *mm, size_t size);
bool memmap_alloc_range(uint64_t base, uint64_t length, uint32_t type, uint32_t overlay_type, bool panic, bool simulation, bool new_entry);
void pmm_randomise_memory(void);

void *ext_mem_alloc(size_t count);
void *ext_mem_alloc_type(size_t count, uint32_t type);
void *ext_mem_alloc_type_aligned(size_t count, uint32_t type, size_t alignment);

void *conv_mem_alloc(size_t count);

void pmm_free(void *ptr, size_t length);

#if defined (UEFI)
void pmm_reclaim_uefi_mem(void);
void pmm_release_uefi_mem(void);
#endif

#endif
