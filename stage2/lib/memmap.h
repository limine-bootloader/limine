#ifndef __LIB__MEMMAP_H__
#define __LIB__MEMMAP_H__

#include <stdint.h>
#include <sys/e820.h>

#define MEMMAP_USABLE                 1
#define MEMMAP_RESERVED               2
#define MEMMAP_ACPI_RECLAIMABLE       3
#define MEMMAP_ACPI_NVS               4
#define MEMMAP_BAD_MEMORY             5
#define MEMMAP_BOOTLOADER_RECLAIMABLE 0x1000
#define MEMMAP_KERNEL_AND_MODULES     0x1001

void init_memmap(void);
void *ext_mem_balloc(size_t count, uint32_t type);
void *ext_mem_balloc_aligned(size_t count, size_t alignment, uint32_t type);
void memmap_alloc_range(uint64_t base, uint64_t length, uint32_t type);
struct e820_entry_t *get_memmap(size_t *entries);
void print_memmap(struct e820_entry_t *mm, size_t size);

#endif
