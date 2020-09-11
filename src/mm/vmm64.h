#ifndef __MM__VMM64_H__
#define __MM__VMM64_H__

#include <stdint.h>

#define PAGE_SIZE ((uint64_t)0x200000)

typedef uint64_t pagemap_t;

pagemap_t new_pagemap(void);
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags);

#endif
