#ifndef __MM__VMM_H__
#define __MM__VMM_H__

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    int   levels;
    void *top_level;
} pagemap_t;

pagemap_t new_pagemap(int lv);
void map_page(pagemap_t pagemap, uint64_t virt_addr, uint64_t phys_addr, uint64_t flags, bool hugepages);

#endif
