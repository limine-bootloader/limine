#ifndef __MM__PMM_H__
#define __MM__PMM_H__

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <sys/e820.h>

#define MEMMAP_USABLE                 1
#define MEMMAP_RESERVED               2
#define MEMMAP_ACPI_RECLAIMABLE       3
#define MEMMAP_ACPI_NVS               4
#define MEMMAP_BAD_MEMORY             5
#define MEMMAP_BOOTLOADER_RECLAIMABLE 0x1000
#define MEMMAP_KERNEL_AND_MODULES     0x1001
#define MEMMAP_FRAMEBUFFER            0x1002
#define MEMMAP_EFI_RECLAIMABLE        0x2000

extern struct e820_entry_t memmap[];
extern size_t memmap_entries;

#if defined (uefi)
extern struct e820_entry_t untouched_memmap[];
extern size_t untouched_memmap_entries;
#endif

void init_memmap(void);
struct e820_entry_t *get_memmap(size_t *entries);
struct e820_entry_t *get_raw_memmap(size_t *entry_count);
void print_memmap(struct e820_entry_t *mm, size_t size);
bool memmap_alloc_range(uint64_t base, uint64_t length, uint32_t type, bool free_only, bool panic, bool simulation, bool new_entry);

void *ext_mem_alloc(size_t count);
void *ext_mem_alloc_type(size_t count, uint32_t type);

void *conv_mem_alloc(size_t count);

#if defined (uefi)
void pmm_reclaim_uefi_mem(void);
void pmm_release_uefi_mem(void);
#endif

#endif
