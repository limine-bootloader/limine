#ifndef __LIB__MEMMAP_H__
#define __LIB__MEMMAP_H__

#include <stdint.h>
#include <lib/e820.h>

void init_memmap(void);
void memmap_alloc_range(uint64_t base, uint64_t length);
struct e820_entry_t *get_memmap(size_t *entries);
void print_memmap(struct e820_entry_t *mm, size_t size);

#endif
