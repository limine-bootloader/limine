#ifndef __LIB__MEMMAP_H__
#define __LIB__MEMMAP_H__

#include <stdint.h>
#include <sys/e820.h>

void init_memmap(void);
void *ext_mem_balloc(size_t count);
void *ext_mem_balloc_aligned(size_t count, size_t alignment);
void memmap_alloc_range(uint64_t base, uint64_t length, uint32_t type);
struct e820_entry_t *get_memmap(size_t *entries);
void print_memmap(struct e820_entry_t *mm, size_t size);

#endif
